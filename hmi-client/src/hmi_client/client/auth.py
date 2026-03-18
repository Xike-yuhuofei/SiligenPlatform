"""Permission management for Siligen HMI."""
from dataclasses import dataclass
from typing import Optional, Dict
import time


@dataclass
class User:
    """User data structure."""
    username: str
    role: str
    level: int


# Hardcoded users for MVP
USERS: Dict[str, tuple] = {
    "operator": ("1234", "操作员", 1),
    "tech": ("5678", "技术员", 2),
    "engineer": ("9999", "工程师", 3),
}

# Permission map: widget pattern -> required level
PERMISSION_MAP = {
    "btn-production-start": 1,
    "btn-production-pause": 1,
    "btn-production-stop": 1,
    "btn-recipe-load": 1,
    "btn-jog": 2,
    "btn-home": 2,
    "input-recipe": 2,
    "btn-recipe-save": 2,
    "btn-dispenser": 2,
    "btn-supply": 2,
    "btn-purge": 2,
    "btn-move": 2,
    "input-move": 2,
    "btn-dxf": 2,
    "input-dxf": 2,
    "slider-jog": 2,
    "btn-speed": 2,
    "btn-recipe-delete": 3,
    "btn-hw-init": 3,
}


class AuthManager:
    """Manages user authentication and permissions."""

    def __init__(self, auto_logout_minutes: int = 5):
        self._current_user: Optional[User] = None
        self._last_activity = time.time()
        self._auto_logout_seconds = auto_logout_minutes * 60

    @property
    def current_user(self) -> Optional[User]:
        return self._current_user

    @property
    def is_logged_in(self) -> bool:
        return self._current_user is not None

    def login(self, username: str, pin: str) -> tuple[bool, str]:
        """Authenticate user with PIN."""
        if username not in USERS:
            return False, "用户不存在"
        stored_pin, role, level = USERS[username]
        if pin != stored_pin:
            return False, "密码错误"
        self._current_user = User(username, role, level)
        self._last_activity = time.time()
        return True, f"欢迎, {role}"

    def logout(self):
        """Log out current user."""
        self._current_user = None

    def check_activity(self) -> bool:
        """Check for auto-logout. Returns True if still active."""
        if not self._current_user:
            return False
        if time.time() - self._last_activity > self._auto_logout_seconds:
            self._current_user = User("operator", "操作员", 1)
            return False
        return True

    def record_activity(self):
        """Record user activity to reset auto-logout timer."""
        self._last_activity = time.time()

    def check_permission(self, widget_id: str) -> bool:
        """Check if current user has permission for widget."""
        if not self._current_user:
            return False
        for pattern, required_level in PERMISSION_MAP.items():
            if widget_id.startswith(pattern):
                return self._current_user.level >= required_level
        return True  # Allow if not in map
