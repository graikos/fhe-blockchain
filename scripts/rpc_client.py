import sys
import socket
import json
import subprocess
import readline


def load_config(config_path):
    with open(config_path) as f:
        return json.load(f)


def send_message(config, msg):
    client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    client.connect((config["net"]["rpc_address"], config["net"]["rpc_port"]))

    msg_json = json.dumps(msg)
    s = len(msg_json).to_bytes(4, "big")

    client.sendall(s)
    client.sendall(msg_json.encode())

    response = receive_response(client)
    client.close()
    return response


def receive_response(client):
    buffer = ""
    while True:
        data = client.recv(2048)
        if not data:
            break
        buffer += data.decode("utf-8")
        try:
            json_resp = json.loads(buffer)
            return json_resp
        except json.JSONDecodeError:
            continue


def send_hello(config):
    msg = {"type": 0, "message": "hello"}
    response = send_message(config, msg)
    print("Server response:", response)


def send_transaction(config):
    transaction_details = handle_transaction()
    response = send_message(config, transaction_details)
    print("Server response:", response)


def handle_transaction():
    recipient_public_key = input("Enter the recipient's public key: ")

    while True:
        amount = input("Enter the transaction amount (must be a positive integer): ")
        if amount.isdigit() and int(amount) > 0:
            amount = int(amount)
            break
        else:
            print(
                "Invalid input. Please enter a valid positive integer for the amount."
            )

    while True:
        fee = input(
            "Enter the transaction fee (must be a non-negative integer, can be zero): "
        )
        if fee.isdigit() and int(fee) >= 0:
            fee = int(fee)
            break
        else:
            print(
                "Invalid input. Please enter a valid non-negative integer for the fee."
            )

    transaction = {
        "type": 1,
        "recipient_public_key": recipient_public_key,
        "amount": amount,
        "fee": fee,
    }
    return transaction


def send_computation(config):
    filename = input("Enter the JSON file name: ")

    try:
        with open(filename, "r") as file:
            computation_data = json.load(file)

        response = send_message(config, computation_data)
        print("Server response:", response)

    except FileNotFoundError:
        print(f"File {filename} not found.")
    except json.JSONDecodeError:
        print("Invalid JSON format in the file.")


def send_output(config):
    # Ask for the block height and private key filename
    while True:
        block_height = input("Enter the block height (>= 0): ")
        if block_height.isdigit() and int(block_height) >= 0:
            block_height = int(block_height)
            break
        else:
            print(
                "Invalid input. Please enter a valid non-negative integer for the block height."
            )

    # Ask for the computation index
    while True:
        computation_index = input("Enter the computation index (>= 0): ")
        if computation_index.isdigit() and int(computation_index) >= 0:
            computation_index = int(computation_index)
            break
        else:
            print(
                "Invalid input. Please enter a valid non-negative integer for the computation index."
            )

    private_key_filename = input("Enter the private key filename: ")

    # Create the message to be sent to the server
    msg = {
        "type": 3,
        "block_height": block_height,
        "computation_index": computation_index,
    }

    response = send_message(config, msg)

    if response.get("status") == 200:
        output_data = response.get("output", "")
        if output_data:
            process = subprocess.Popen(
                ["./decryptor", private_key_filename],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                text=True,
            )

            stdout, _ = process.communicate(input=output_data)

            print("Computation result: ")
            print(stdout)

        else:
            print("No 'output' field found in the response.")
    else:
        print(
            f"Server returned status {response.get('status')}: {response.get('message')}"
        )


def main():
    config_path = "../config/config.json"
    config = load_config(config_path)

    if len(sys.argv) == 3:
        config["net"]["rpc_address"] = sys.argv[1]
        config["net"]["rpc_port"] = int(sys.argv[2])

    command_map = {
        "hello": send_hello,
        "transaction": send_transaction,
        "computation": send_computation,
        "output": send_output,
        "exit": lambda config: print("Exiting the terminal UI."),
    }

    while True:
        print("\nAvailable commands:")
        for command in command_map:
            print(f" - {command}")

        user_input = input("\nEnter a command: ").strip().lower()

        if user_input in command_map:
            if user_input == "exit":
                command_map[user_input](config)
                break
            else:
                command_map[user_input](config)
        else:
            print("Invalid input. Please enter a valid command.")


if __name__ == "__main__":
    main()
