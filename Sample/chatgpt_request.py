import openai
import sys


def ask_chatgpt(prompt: str, api_key: str) -> str:
    openai.api_key = api_key
    response = openai.chat.completions.create(
        model="gpt-4o-mini",
        messages=[{"role": "user", "content": prompt}],
        max_tokens=4000,
        temperature=0.7
    )
    return response.choices[0].message.content


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("No prompt file provided")
        sys.exit(1)

    prompt_file = sys.argv[1]
    with open(prompt_file, "r", encoding="utf-8") as f:
        prompt = f.read()

    apikey = sys.argv[2]

    print(ask_chatgpt(prompt, apikey))
