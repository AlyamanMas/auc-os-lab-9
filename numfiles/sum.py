import sys

def main():
    # Check if the command line argument for the file path is provided
    if len(sys.argv) < 2:
        print("Usage: python sum_numbers.py <file_path>")
        sys.exit(1)

    file_path = sys.argv[1]

    try:
        total_sum = 0
        with open(file_path, 'r') as file:
            for line in file:
                # Attempt to convert the line to an integer and add it to the sum
                try:
                    total_sum += int(line.strip())
                except ValueError:
                    # Handle the case where the line is not a valid integer
                    print(f"Warning: Skipping invalid number '{line.strip()}'")
        
        print(f"Total sum of all numbers: {total_sum}")
    except FileNotFoundError:
        print(f"Error: The file '{file_path}' does not exist.")
    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    main()
