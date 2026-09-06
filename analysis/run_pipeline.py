"""Run feature engineering, model validation/publication, and prediction."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from feature_engineering import build_features
from predict import generate_predictions
from train_models import train_models


def run_pipeline(database: Path, output: Path) -> dict:
    features = build_features(database, output)
    models = train_models(output)
    predictions = generate_predictions(output)
    return {
        "features": features,
        "models": {name: details["accepted"]
                   for name, details in models["models"].items()},
        "predictions": predictions,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--database", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("analysis"))
    args = parser.parse_args()
    print(json.dumps(run_pipeline(args.database, args.output), ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
