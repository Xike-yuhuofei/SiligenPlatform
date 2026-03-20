"""Legacy service shell preserved inside engineering-data."""

from .dxf_preprocessing import (
    DXFPreprocessingService,
    PreprocessingConfig,
    PreprocessingResult,
    ValidationResult,
)
from .migration_adapter import MigrationAdapter

__all__ = [
    "DXFPreprocessingService",
    "MigrationAdapter",
    "PreprocessingConfig",
    "PreprocessingResult",
    "ValidationResult",
]
