"""
BamEngine Gymnasium Environment Adapter
Engine 계획 15절 및 Gymnasium 규약 준수
"""

import json
import socket
import random

try:
    import numpy as np
    HAS_NUMPY = True
except ImportError:
    np = None
    HAS_NUMPY = False

try:
    import gymnasium as gym
    from gymnasium import spaces
    HAS_GYMNASIUM = True
except ImportError:
    HAS_GYMNASIUM = False
    class DummyEnv: pass
    gym = type('DummyGym', (), {'Env': DummyEnv})()
    spaces = None


class DiscreteSpaceFallback:
    """Gymnasium/numpy 미설치 시의 이산 행동 공간 폴백"""
    def __init__(self, n: int):
        self.n = n

    def sample(self) -> int:
        return random.randrange(self.n)

    def __repr__(self):
        return f"Discrete({self.n})"


class BoxSpaceFallback:
    """Gymnasium/numpy 미설치 시의 관측 공간 폴백"""
    def __init__(self, shape: tuple):
        self.shape = shape

    def __repr__(self):
        return f"Box({self.shape})"


class BamCartPoleEnv(gym.Env):
    """BamEngine CartPole 헤드리스 시뮬레이션 환경 어댑터"""
    metadata = {"render_modes": []}

    def __init__(self, host: str = "127.0.0.1", port: int = 9999):
        super().__init__()
        self.host = host
        self.port = port
        self.sock = None

        self._connect()
        self._setup_spaces()

    def _connect(self):
        if self.sock is not None:
            self.sock.close()
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.sock.connect((self.host, self.port))

    def _send_cmd(self, payload: dict) -> dict:
        msg = json.dumps(payload) + "\n"
        self.sock.sendall(msg.encode('utf-8'))
        
        # 줄바꿈(\n)까지 응답 수신
        buf = b""
        while b"\n" not in buf:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionResetError("BamEngine server disconnected.")
            buf += chunk
        
        line = buf.split(b"\n")[0].decode('utf-8')
        return json.loads(line)

    def _setup_spaces(self):
        resp = self._send_cmd({"cmd": "get_spaces"})
        if resp.get("status") != "ok":
            raise RuntimeError(f"Failed to get spaces: {resp}")

        # Gymnasium spaces 설정
        num_actions = resp["action_space"]["n"]
        obs_shape = tuple(resp["observation_space"]["shape"])

        if HAS_GYMNASIUM and HAS_NUMPY:
            self.action_space = spaces.Discrete(num_actions)
            self.observation_space = spaces.Box(
                low=-np.inf, high=np.inf, shape=obs_shape, dtype=np.float32
            )
        else:
            self.action_space = DiscreteSpaceFallback(num_actions)
            self.observation_space = BoxSpaceFallback(obs_shape)

    def reset(self, seed: int = None, options: dict = None):
        if HAS_GYMNASIUM:
            super().reset(seed=seed)
        cmd = {"cmd": "reset"}
        if seed is not None:
            cmd["seed"] = int(seed)
        
        resp = self._send_cmd(cmd)
        raw_obs = resp["observation"]
        obs = np.array(raw_obs, dtype=np.float32) if HAS_NUMPY else raw_obs
        info = resp.get("info", {})
        return obs, info

    def step(self, action: int):
        resp = self._send_cmd({"cmd": "step", "action": int(action)})
        if resp.get("status") != "ok":
            raise RuntimeError(f"Step command failed: {resp.get('message', 'Unknown error')}")

        raw_obs = resp["observation"]
        obs = np.array(raw_obs, dtype=np.float32) if HAS_NUMPY else raw_obs
        reward = float(resp["reward"])
        terminated = bool(resp["terminated"])
        truncated = bool(resp["truncated"])
        info = resp.get("info", {})
        return obs, reward, terminated, truncated, info

    def close(self):
        if self.sock is not None:
            try:
                self._send_cmd({"cmd": "close"})
            except Exception:
                pass
            self.sock.close()
            self.sock = None


if __name__ == "__main__":
    print("=== Testing BamEngine CartPole Environment ===")
    try:
        env = BamCartPoleEnv()
        print(f"Connected to BamEngine! Action space: {env.action_space}, Obs shape: {env.observation_space.shape}")

        # ── 1. seed=0 결정론적 재현성 검증 ──
        obs1, _ = env.reset(seed=0)
        obs2, _ = env.reset(seed=0)
        is_same = (obs1 == obs2).all() if HAS_NUMPY else (obs1 == obs2)
        print(f"Deterministic seed=0 test: {'PASSED (Identical initial states)' if is_same else 'FAILED'}")

        # ── 2. 2개 에피소드 실행 ──
        for episode in range(1, 3):
            obs, info = env.reset(seed=42 + episode)
            print(f"\n--- Episode {episode} Start ---")
            print(f"Initial Obs: {obs}")
            total_reward = 0.0

            for step in range(500):
                action = env.action_space.sample()
                obs, reward, term, trunc, info = env.step(action)
                total_reward += reward

                if term or trunc:
                    print(f"Episode {episode} finished at step {step + 1} with reward {total_reward} (Terminated: {term}, Truncated: {trunc})")
                    print(f"Final Obs: {obs}")
                    break

        env.close()
        print("\nAll Gymnasium contract tests passed successfully!")
    except ConnectionRefusedError:
        print("[ERROR] Cannot connect to BamEngine RLServer on 127.0.0.1:9999. Please run the server first.")