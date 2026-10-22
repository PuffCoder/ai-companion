import os
import pytest


@pytest.fixture
def deepseek_key():
    key = os.environ.get("DEEPSEEK_API_KEY")
    if not key:
        pytest.skip("DEEPSEEK_API_KEY not set")
    return key


@pytest.fixture
def groq_key():
    key = os.environ.get("GROQ_API_KEY")
    if not key:
        pytest.skip("GROQ_API_KEY not set")
    return key
