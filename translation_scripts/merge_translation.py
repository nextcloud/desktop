import sys
import subprocess
import xml.etree.ElementTree as ET

def load_source_messages(source_file):
    """Load messages from the source XML file into a dictionary organized by context."""
    tree = ET.parse(source_file)
    root = tree.getroot()
    messages = {}
    
    for context in root.findall("context"):
        context_name = context.find("name").text if context.find("name") is not None else ""
        if context_name not in messages:
            messages[context_name] = {}
        
        for message in context.findall("message"):
            source = message.find("source")
            translation = message.find("translation")
            if source is not None and translation is not None:
                messages[context_name][source.text] = translation.text
    
    return messages

def update_target_messages(target_file, source_messages):
    """Update messages in the target XML file and insert missing ones in the correct context."""
    tree = ET.parse(target_file)
    root = tree.getroot()
    
    existing_messages = {}
    for context in root.findall("context"):
        context_name = context.find("name").text if context.find("name") is not None else ""
        if context_name not in existing_messages:
            existing_messages[context_name] = set()
        
        for message in context.findall("message"):
            source = message.find("source")
            translation = message.find("translation")
            if source is not None and translation is not None:
                existing_messages[context_name].add(source.text)
                if context_name in source_messages and source.text in source_messages[context_name]:
                    translation.text = source_messages[context_name][source.text]
                    translation.attrib.pop("type", None)  # Remove type="unfinished" attribute
    
    # Insert missing messages into corresponding contexts
    for context_name, messages in source_messages.items():
        target_context = None
        for context in root.findall("context"):
            if (context.find("name") is not None and context.find("name").text == context_name):
                target_context = context
                break
        
        if target_context is None:
            target_context = ET.SubElement(root, "context")
            context_name_elem = ET.SubElement(target_context, "name")
            context_name_elem.text = context_name
        
        for source_text, translation_text in messages.items():
            if source_text not in existing_messages.get(context_name, set()):
                message = ET.SubElement(target_context, "message")
                source = ET.SubElement(message, "source")
                source.text = source_text
                translation = ET.SubElement(message, "translation")
                translation.text = translation_text
    
    tree.write(target_file, encoding="utf-8", xml_declaration=True)
    
def merge(source, target):
    source_messages = load_source_messages(source)
    update_target_messages(target, source_messages)

def pop_vanished(target_file):
    tree = ET.parse(target_file)
    root = tree.getroot()
    
       
    for context in root.findall("context"):
        for message in context.findall("message"):
            source = message.find("source")
            translation = message.find("translation")
            
            if translation.attrib.get("type") == "vanished":
                translation.attrib.pop("type")  # Remove type="vanished" attribute
        
    tree.write(target_file, encoding="utf-8", xml_declaration=True)

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

def run_script(script_name, input_file, output_file):
    try:
        # Running each script with input and output file arguments
        subprocess.run(['python', script_name, input_file, output_file], check=True)
        # print(f"{script_name} executed successfully with input '{input_file}' and output '{output_file}'.")
    except subprocess.CalledProcessError as e:
        print(f"An error occurred while running {script_name} with input '{input_file}' and output '{output_file}': {e}")

def run_lupdate(ts_files, mode="default"):
    command = [
        r"C:\Craft64\bin\lupdate.exe",
        "-locations", "none",
        "-no-ui-lines",
        "-no-sort",
        r"..\src\libsync",
        r"..\src\gui",
        r"..\src\csync",
        r"..\src\common",
        r"..\src\cmd",
        "-ts"
    ]
    
    command_no_obs = [
        r"C:\Craft64\bin\lupdate.exe",
        "-locations", "none",
        "-no-ui-lines",
        "-no-sort",
        "-no-obsolete",
        r"..\src\libsync",
        r"..\src\gui",
        r"..\src\csync",
        r"..\src\common",
        r"..\src\cmd",
        "-ts"
    ]
    
    command_location = [
        r"C:\Craft64\bin\lupdate.exe",
        "-locations", "absolute",
        "-no-sort",
        "-no-obsolete",
        r"..\src\libsync",
        r"..\src\gui",
        r"..\src\csync",
        r"..\src\common",
        r"..\src\cmd",
        "-ts"
    ]

    # Append all ts_files paths to the command list
    command.extend(ts_files)
    command_no_obs.extend(ts_files)
    command_location.extend(ts_files)

    try:
        if mode == "default":
            result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, check=True)
        elif mode == "no_obs":
            result = subprocess.run(command_no_obs, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, check=True)
        elif mode == "location":
            result = subprocess.run(command_location, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, check=True)
        print("Output:", result.stdout)
    except subprocess.CalledProcessError as e:
        print("Error:", e.stderr)


if __name__ == "__main__":
    ts_files = [r"..\translations\client_de.ts"
                , r"..\translations\client_es.ts"
                , r"..\translations\client_fr.ts"
                , r"..\translations\client_nl.ts"
                , r"..\translations\client_en.ts"
                , r"..\translations\client_en_GB.ts"
                ]
    
    diff_files = [r".\de_DE.ts"
                  , r".\es.ts"
                  , r".\fr.ts"
                  , r".\nl.ts"
                  , r".\en.ts"
                  , r".\en_GB.ts"
                  ]

    if len(sys.argv) == 1:
        print("Usage: python merge.py <input_file> <output_file>")
        sys.exit()
    if len(sys.argv) == 3:
        print("Usage: python remove_line_attributes.py <input_file> <output_file>")
        sys.exit()
    if len(sys.argv) == 2:
        step = sys.argv[1]
    try:
        
        if step == "0" or step == "all":
            # Step 0 full sort
            for ts_file in ts_files:      
                sort_and_repair(ts_file)  
            print("Step 0 completed: sorted")
        
        if step == "1" or step == "all":
            # Step 1 lupdate Nextcloud in our latest change state, keep obsolete
            run_lupdate(ts_files)
        
            # Step 2 full sort
            for ts_file in ts_files:     
                pop_vanished(ts_file)   
                sort_and_repair(ts_file)  
               
            print("Step 1 completed: lupdate (keep obsolete), pop vanished, full sort, you should commit now")
                                 
        if step == "2" or step == "all":            
            run_lupdate(ts_files, "no_obs")
            for ts_file in ts_files:        
                sort_and_repair(ts_file)  
            print("Step 2 completed: lupdate removed obsolete, you should commit now")
                
            # Step 3 merge diff into
        if step == "3" or step == "all":
           
            for ts_file in ts_files:        
                merge(diff_files[ts_files.index(ts_file)], ts_file)
                sort_and_repair(ts_file)  
            print("Step 3 completed: merged, sorted, you should commit now")

        if step == "4" or step == "all":            
            run_lupdate(ts_files)
            for ts_file in ts_files:        
                pop_vanished(ts_file)   
                sort_and_repair(ts_file)  
            print("Step 4 completed: lupdate fill duplicates, format, you should commit now")
            
        if step == "5" or step == "all":            
            run_lupdate(ts_files, "location")
            for ts_file in ts_files:        
                sort_and_repair(ts_file)  
            print("Step 5 completed: lupdate remove obsolete, sort, you should commit now")

    except Exception as e:
        print(f"An error occurred: {e}")

