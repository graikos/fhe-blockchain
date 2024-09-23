#ifndef DIPLO_AST_HPP
#define DIPLO_AST_HPP

#include <deque>
#include <string>
#include <memory>
#include <stack>

class ASTLeaf;

class ASTNode
{
public:
    bool is_leaf;
    int val_;
    int node_id_;
    std::shared_ptr<ASTNode> parent_;
    std::shared_ptr<ASTNode> left_child_;
    std::shared_ptr<ASTNode> right_child_;
    std::string op_;

    int depth_;
    bool counts_for_depth_;

    ASTNode(int id, bool counts_for_depth, std::shared_ptr<ASTNode> parent, std::string op);
    ASTNode(int id, int val, std::shared_ptr<ASTNode> parent);

    bool insert_child(std::shared_ptr<ASTNode> child);
    bool can_rotate_left();
    bool can_rotate_right();
    bool is_full();

    void notify_child_change(std::shared_ptr<ASTNode> new_child, int old_child_id);
};

class ASTree
{
public:
    int node_counter_;
    ASTree(const std::string &expression);
    ASTree(const std::deque<std::string> &input);
    std::shared_ptr<ASTNode> root_;
    std::stack<std::shared_ptr<ASTNode>> node_stack;

    void bfs_print();

    int get_balance(std::shared_ptr<ASTNode> node);
    int get_depth(std::shared_ptr<ASTNode> node);

    std::shared_ptr<ASTNode> right_rotate(std::shared_ptr<ASTNode> node);
    std::shared_ptr<ASTNode> left_rotate(std::shared_ptr<ASTNode> node);

private:
    const std::deque<std::string> input_;

    void build_tree();
    std::deque<std::string> shunt(const std::string &expression);
    

    std::shared_ptr<ASTNode> reorg_from(std::shared_ptr<ASTNode> node);
};

#endif
