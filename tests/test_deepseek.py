import requests


def test_deepseek_responds(deepseek_key):
    """DeepSeek API 能正常返回回复"""
    r = requests.post(
        "https://api.deepseek.com/chat/completions",
        headers={"Authorization": f"Bearer {deepseek_key}"},
        json={
            "model": "deepseek-chat",
            "messages": [{"role": "user", "content": "say hi"}],
            "max_tokens": 20,
        },
        timeout=15,
    )
    assert r.status_code == 200, f"HTTP {r.status_code}: {r.text}"
    data = r.json()
    assert "choices" in data
    assert data["choices"][0]["message"]["content"]
    print("DeepSeek reply:", data["choices"][0]["message"]["content"])


def test_deepseek_invalid_key():
    """无效 key 应返回 401"""
    r = requests.post(
        "https://api.deepseek.com/chat/completions",
        headers={"Authorization": "Bearer invalid-key"},
        json={
            "model": "deepseek-chat",
            "messages": [{"role": "user", "content": "hi"}],
        },
        timeout=15,
    )
    assert r.status_code == 401
