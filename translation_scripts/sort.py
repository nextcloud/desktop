import sys
import subprocess
import xml.etree.ElementTree as ET

def replace_in_file(file_path):
    try:
        # Open the file, read its contents, and replace the string
        with open(file_path, 'r', encoding='utf-8', newline='') as file:
            content = file.read()
        
        # Replace the old string with the new string
        updated_content = content.replace("&amp;quot;", "&quot;").replace("&amp;apos;", "&apos;")
        
        # Write the updated content back to the file
        with open(file_path, 'w', encoding='utf-8', newline='') as file:
            file.write(updated_content)
        
        # print(f"Successfully replaced '{old_string}' with '{new_string}' in '{file_path}'.")
    except FileNotFoundError:
        print(f"Error: File '{file_path}' not found.")
    except Exception as e:
        print(f"An error occurred: {e}")

def replace_quotes_and_apostrophes(text):
    """Replace quotes and apostrophes with their HTML encodings."""
    # return html.escape(text)
    # return text.replace('"', '&quot;')
    return text.replace('"', '&quot;').replace("'", '&apos;')

def process_file(input_file, output_file):
    try:
        # Parse the XML file
        tree = ET.parse(input_file)
        root = tree.getroot()

        # Process <source> and <translation> tags
        for tag in root.findall(".//source") + root.findall(".//translation") + root.findall(".//extracomment"):
            if tag.text:  # Check if there's any text inside the tag
                tag.text = replace_quotes_and_apostrophes(tag.text)

        # Write the updated XML to the output file
        with open(output_file, "w", encoding="utf-8", newline="\n") as outfile:
            tree.write(outfile, encoding="unicode", xml_declaration=True)

        # with open(output_file, "wb", newline="") as f:
        #     tree.write(f, encoding="utf-8", xml_declaration=True)
        # # tree.write(output_file, encoding="utf-8", xml_declaration=True)
    except ET.ParseError as e:
        print(f"Error parsing XML file: {e}")
    except Exception as e:
        print(f"An error occurred: {e}")

def repair_file(file):
    process_file(file, file)
    replace_in_file(file)

def sort_contexts(input_file, output_file):
    # Parse the XML file
    tree = ET.parse(input_file)
    root = tree.getroot()

    # Find all <context> elements
    contexts = root.findall("context")

    # Sort <context> elements based on the text inside their <name> child
    def get_name_text(ctx):
        name_element = ctx.find("name")
        # If <name> is missing or its text is None, treat it as an empty string
        return name_element.text.strip().lower() if name_element is not None and name_element.text else ""

    contexts.sort(key=get_name_text)

    # Remove all existing <context> elements from the root
    for context in root.findall("context"):
        root.remove(context)

    # Append sorted <context> elements back to the root
    for context in contexts:
        root.append(context)

    # Write the updated XML to the output file
    tree.write(output_file, encoding="utf-8", xml_declaration=True) 


def sort_messages(input_file, output_file):
    # Parse the XML file
    tree = ET.parse(input_file)
    root = tree.getroot()

    # Traverse all <context> tags
    for context in root.findall("context"):
        # Find all <message> tags within the current <context>
        messages = context.findall("message")

        # Define the sorting key
        def message_sort_key(message):
            # Get the <location> tag and its filename attribute
            location = message.find("location")
            filename = location.get("filename") if location is not None else ""

            # Get the text of the <source> tag
            source = message.find("source")
            source_text = source.text.strip() if source is not None and source.text else ""

            # Return a tuple for sorting: primary (filename), secondary (source_text)
            return (filename, source_text)

        # Sort messages by the defined key
        messages.sort(key=message_sort_key)

        # Remove existing <message> tags from the <context>
        for message in messages:
            context.remove(message)

        # Re-add sorted <message> tags to the <context>
        for message in messages:
            context.append(message)

    # Write the updated XML to the output file
    tree.write(output_file, encoding="utf-8", xml_declaration=True)

def full_sort(file):
    sort_contexts(file, file)
    sort_messages(file, file)

def sort_and_repair(file):
    full_sort(file)
    repair_file(file)
    

if __name__ == "__main__":
    if len(sys.argv) == 1:
        print("Usage: python sort.py <input_file>")
        sys.exit()
    if len(sys.argv) == 2:
        input_xml = sys.argv[1]
        output_xml = sys.argv[1]
    else:
        sys.exit()
        
    try:
        sort_and_repair(input_xml)
        print(f"{input_xml} sorted.")
    except Exception as e:
        print(f"An error occurred: {e}")