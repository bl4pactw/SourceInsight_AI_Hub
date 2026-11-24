import json
import sys


def is_real_user_turn(entry):
    if entry.get("type") != "user" or entry.get("isMeta"):
        return False
    content = entry.get("message", {}).get("content")
    if isinstance(content, str):
        return bool(content.strip())
    if isinstance(content, list):
        return any(isinstance(block, dict) and block.get("type") == "text" for block in content)
    return False


def assistant_texts(entry):
    if entry.get("type") != "assistant":
        return []
    content = entry.get("message", {}).get("content")
    if isinstance(content, str):
        return [content] if content.strip() else []
    texts = []
    for block in content or []:
        if isinstance(block, dict) and block.get("type") == "text" and block.get("text", "").strip():
            texts.append(block["text"])
    return texts


def main():
    with open(sys.argv[1], encoding="utf-8") as marker_file:
        transcript_path = json.load(marker_file).get("transcript_path", "")
    entries = []
    with open(transcript_path, encoding="utf-8") as transcript_file:
        for line in transcript_file:
            try:
                entries.append(json.loads(line))
            except ValueError:
                continue
    last_user = -1
    for index, entry in enumerate(entries):
        if is_real_user_turn(entry):
            last_user = index
    texts = []
    for entry in entries[last_user + 1:]:
        texts.extend(assistant_texts(entry))
    sys.stdout.write("\n\n".join(texts))


main()
