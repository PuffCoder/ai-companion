import requests, ujson
from config import API_KEY

_history = []

SYSTEM_PROMPT = "你是AI机器人小黄人，性格活泼可爱，回答简短。"

def chat(msg):
    """发送消息给 GPT，返回回复"""
    _history.append({"role": "user", "content": msg})

    resp = requests.post(
        "https://api.openai.com/v1/chat/completions",
        headers={
            "Authorization": "Bearer " + API_KEY,
            "Content-Type": "application/json"
        },
        data=ujson.dumps({
            "model": "gpt-4o-mini",
            "max_tokens": 150,
            "messages": [{"role": "system", "content": SYSTEM_PROMPT}] + _history
        }).encode("utf-8")
    )
    reply = ujson.loads(resp.text)["choices"][0]["message"]["content"]
    resp.close()

    _history.append({"role": "assistant", "content": reply})
    print("AI:", reply)
    return reply

def clear_history():
    """清空对话历史"""
    _history.clear()
    print("对话历史已清空")
