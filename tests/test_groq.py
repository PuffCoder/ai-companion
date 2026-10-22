import requests


def test_groq_models_reachable(groq_key):
    """Groq API 可以访问，key 有效"""
    r = requests.get(
        "https://api.groq.com/openai/v1/models",
        headers={"Authorization": f"Bearer {groq_key}"},
        timeout=15,
    )
    assert r.status_code == 200, f"HTTP {r.status_code}: {r.text}"
    data = r.json()
    assert "data" in data
    models = [m["id"] for m in data["data"]]
    print("Available models:", models)


def test_groq_whisper_model_available(groq_key):
    """whisper-large-v3-turbo 模型存在"""
    r = requests.get(
        "https://api.groq.com/openai/v1/models",
        headers={"Authorization": f"Bearer {groq_key}"},
        timeout=15,
    )
    assert r.status_code == 200
    models = [m["id"] for m in r.json()["data"]]
    assert "whisper-large-v3-turbo" in models, \
        f"whisper-large-v3-turbo not found, available: {models}"
