#include <iostream>
#include "computer/ast.hpp"
#include <stack>
#include <unordered_map>

ASTNode::ASTNode(int id, bool counts_for_depth, std::shared_ptr<ASTNode> parent, std::string op) : is_leaf(false), val_(0), node_id_(id), parent_(parent), left_child_(nullptr), right_child_(nullptr), op_(op), counts_for_depth_(counts_for_depth)
{
    if (counts_for_depth)
    {
        depth_ = 1;
    }
    else
    {
        depth_ = 0;
    }
}

ASTNode::ASTNode(int id, int val, std::shared_ptr<ASTNode> parent) : is_leaf(true), val_(val), node_id_(id), parent_(parent), left_child_(nullptr), right_child_(nullptr), depth_(0), counts_for_depth_(false)
{
}

bool ASTNode::is_full()
{
    return right_child_ && left_child_;
}

void ASTNode::notify_child_change(std::shared_ptr<ASTNode> new_child, int old_child_id)
{
    if (right_child_ && right_child_->node_id_ == old_child_id)
    {
        right_child_ = new_child;
    }
    else if (left_child_ && left_child_->node_id_ == old_child_id)
    {
        left_child_ = new_child;
    }
}

bool ASTNode::insert_child(std::shared_ptr<ASTNode> child)
{
    // priority will be to insert to the right
    if (!right_child_)
    {
        right_child_ = child;
        return false;
    }
    left_child_ = child;
    return true;
}

bool ASTNode::can_rotate_left()
{
    // for left rotation, this node needs to be "*" and right child too
    return (right_child_ && right_child_->op_ == "*" && op_ == "*");
}

bool ASTNode::can_rotate_right()
{
    // for right rotation, this node needs to be "*" and left child too
    return (left_child_ && left_child_->op_ == "*" && op_ == "*");
}

ASTree::ASTree(const std::string &expression) : node_counter_(0), root_(nullptr), input_(shunt(expression))
{
    build_tree();
}

ASTree::ASTree(const std::deque<std::string> &input) : node_counter_(0), root_(nullptr), input_(input)
{
    build_tree();
}

int ASTree::get_depth(std::shared_ptr<ASTNode> node)
{
    if (!node)
    {
        return 0;
    }

    return node->depth_;
}

int ASTree::get_balance(std::shared_ptr<ASTNode> node)
{
    if (!node)
    {
        return 0;
    }

    return get_depth(node->left_child_) - get_depth(node->right_child_);
}

std::shared_ptr<ASTNode> ASTree::right_rotate(std::shared_ptr<ASTNode> node)
{
    auto x = node->left_child_;
    auto x_r = x->right_child_;

    x->right_child_ = node;
    node->left_child_ = x_r;

    x->parent_ = node->parent_;
    if (node->parent_)
    {
        node->parent_->notify_child_change(x, node->node_id_);
    }
    node->parent_ = x;

    node->depth_ = 1 + std::max(get_depth(node->left_child_), get_depth(node->right_child_));
    x->depth_ = 1 + std::max(get_depth(x->left_child_), get_depth(x->right_child_));

    // return the new root of the subtree
    return x;
}

std::shared_ptr<ASTNode> ASTree::left_rotate(std::shared_ptr<ASTNode> node)
{
    auto x = node->right_child_;
    auto x_l = x->left_child_;

    x->left_child_ = node;
    node->right_child_ = x_l;

    x->parent_ = node->parent_;
    if (node->parent_)
    {
        node->parent_->notify_child_change(x, node->node_id_);
    }
    node->parent_ = x;

    node->depth_ = 1 + std::max(get_depth(node->left_child_), get_depth(node->right_child_));
    x->depth_ = 1 + std::max(get_depth(x->left_child_), get_depth(x->right_child_));

    // return the new root of the subtree
    return x;
}

void ASTree::build_tree()
{
    // std::stack<std::shared_ptr<ASTNode>> node_stack;
    std::shared_ptr<ASTNode> curr_node;
    for (auto it = input_.rbegin(); it != input_.rend(); ++it)
    {
        // start from reverse input
        // when operator is found, a new node must be added to the most recent node with available spots
        // a stack is used for this

        if (!node_stack.empty())
        {
            curr_node = node_stack.top();
            while (curr_node->is_full())
            {
                node_stack.pop();
                curr_node = node_stack.top();
            }
        }


        std::shared_ptr<ASTNode> new_node;

        if (*it == "+" || *it == "*" || *it == "-")
        {
            new_node = std::make_shared<ASTNode>(node_counter_++, *it == "*", nullptr, *it);
        }
        else
        {
            new_node = std::make_shared<ASTNode>(node_counter_++, std::stoi(*it), nullptr);
        }

        if (!root_)
        {
            root_ = new_node;
            // root parent is left null
        }
        else
        {
            new_node->parent_ = curr_node;
            bool is_full = curr_node->insert_child(new_node);

            if (new_node->counts_for_depth_)
            {
                root_ = reorg_from(curr_node);
            }

            if (is_full)
            {
                node_stack.pop();
            }
        }
        if (!new_node->is_leaf)
        {
            node_stack.push(new_node);
        }
    }
}

std::shared_ptr<ASTNode> ASTree::reorg_from(std::shared_ptr<ASTNode> node)
{

    std::shared_ptr<ASTNode> curr = node;
    if (node->counts_for_depth_)
    {
        node->depth_ = 1 + std::max(get_depth(node->left_child_), get_depth(node->right_child_));

        int balance = get_balance(node);

        if (node->can_rotate_right() && balance > 1 && get_balance(node->left_child_) > 0)
        {
            // no need for stack adjustment due to the order of insertions. The left child will be full
            // but this will be caught by the is_full check during insert.
            curr = right_rotate(node);
        }
        else if (node->can_rotate_left() && balance < -1 && get_balance(node->right_child_) < 0)
        {
            // no need for stack adjustment. The right child will be full, but this will be
            // caught by the is_full check during insert. The order of the others is correct
            curr = left_rotate(node);
        }
        else if (node->can_rotate_right() && balance > 1 && get_balance(node->left_child_) < 0 && node->left_child_->can_rotate_left())
        {

            // similarly with below, `node` after the rotation has one free spot and should receive the next node
            node_stack.push(node);
            node->left_child_ = left_rotate(node->left_child_);
            curr = right_rotate(node);
        }
        else if (node->can_rotate_left() && balance < -1 && get_balance(node->right_child_) > 0 && node->right_child_->can_rotate_right())
        {
            // next position to insert changes because of this rotation, so node_stack needs to
            // be adjusted

            // in this case the node->right_child_ will once again have one spot left, so it needs to be
            // added to the stack to receive the next node
            // the parent is already there
            node_stack.push(node->right_child_);
            node->right_child_ = right_rotate(node->right_child_);
            curr = left_rotate(node);
        }
    }
    else
    {
        // this node does not count for depth, so just propagate current
        node->depth_ = std::max(get_depth(node->left_child_), get_depth(node->right_child_));
    }

    // move to parent to reorg
    if (curr->parent_)
    {
        return reorg_from(curr->parent_);
    }
    return curr;
}

std::deque<std::string> ASTree::shunt(const std::string &expression)
{

    std::deque<std::string> output_q;

    std::unordered_map<char, int> precedence = {
        {'+', 2},
        {'-', 2},
        {'*', 3},
    };

    std::stack<std::string> number_stack;
    std::stack<std::string> operator_stack;

    auto n = expression.size();
    // indicates if previous token was number
    bool found_num = false;
    bool found_operation_operator = false;

    for (std::size_t i = 0; i < n; ++i)
    {
        char token = expression[i];

        // current token as string
        std::string entry(1, token);
        // found number token
        if (std::isdigit(token))
        {
            found_operation_operator = false;
            // if previous token was number as well, append it to previous token
            if (found_num)
            {
                output_q.back().append(std::move(entry));
            }
            else
            {
                // previous token was not number, push new entry
                output_q.push_back(std::move(entry));
                found_num = true;
            }
            continue;
        }
        found_num = false;

        switch (token)
        {
        case ' ':
            continue;

        case '(':
            operator_stack.push(std::move(entry));
            found_operation_operator = true;
            break;

        case ')':
            found_operation_operator = false;

            // pop operators until first left parenthesis is found
            while (!operator_stack.empty() && operator_stack.top()[0] != '(')
            {
                output_q.push_back(std::move(operator_stack.top()));
                operator_stack.pop();
            }

            // loop stopped before emptying stack, meaning left parenthesis was found
            if (!operator_stack.empty())
            {
                operator_stack.pop();
            }
            else
            {
                // stack empty and no left parenthesis found
                throw std::invalid_argument("Invalid expression syntax: mismatched ')'");
            }
            break;

        case '+':
        case '-':
        case '*':
            if (found_operation_operator)
            {
                throw std::invalid_argument("Invalid expression syntax: cannot have consecutive operations");
            }
            else
            {
                found_operation_operator = true;
            }
            if (i == 0 || i == n - 1)
            {
                throw std::invalid_argument("Invalid expression syntax: cannot start or end with operator.");
            }

            while (!operator_stack.empty() && operator_stack.top()[0] != '(' && precedence[token] <= precedence[operator_stack.top()[0]])
            {
                // here it is assumed that all operations are left associative
                // so even if equal, the top() will be pushed to the output
                output_q.push_back(std::move(operator_stack.top()));
                operator_stack.pop();
            }
            operator_stack.push(std::move(entry));
            break;

        default:
            throw std::invalid_argument("Invalid expression syntax.");
        }
    }

    if (operator_stack.empty())
    {
        throw std::invalid_argument("Invalid expression syntax: no operators found");
    }

    while (!operator_stack.empty())
    {
        if (operator_stack.top()[0] == '(')
        {
            throw std::invalid_argument("Invalid expression syntax: mismatched '('");
        }
        output_q.push_back(std::move(operator_stack.top()));
        operator_stack.pop();
    }

    return output_q;
}

void ASTree::bfs_print()
{
    std::cout << "==============" << std::endl;
    std::deque<std::shared_ptr<ASTNode>> q;
    auto s = root_;
    q.push_back(s);

    std::unordered_map<int, int> discovered;
    discovered[s->node_id_] = 0;
    int curr_disc = 0;

    while (!q.empty())
    {
        auto top = q.front();
        q.pop_front();
        if (discovered[top->node_id_] > curr_disc)
        {
            ++curr_disc;
            std::cout << std::endl;
        }
        if (top->is_leaf)
        {
            std::cout << top->val_;
        }
        else
        {
            std::cout << top->op_;
        }
        std::cout << "  |   ";

        if (top->left_child_)
        {
            discovered[top->left_child_->node_id_] = discovered[top->node_id_] + 1;
            q.push_back(top->left_child_);
        }
        if (top->right_child_)
        {
            discovered[top->right_child_->node_id_] = discovered[top->node_id_] + 1;
            q.push_back(top->right_child_);
        }
    }
    std::cout << std::endl;
}