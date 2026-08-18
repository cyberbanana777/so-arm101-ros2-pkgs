#!/usr/bin/env python3

# Copyright (c) 2026 Alice Zenina and Alexander Grachev RTU MIREA (Russia)
# SPDX-License-Identifier: MIT
# Details in the LICENSE file in the root of the package.

"""
Convert robot arm configuration from JSON to YAML.

Usage:
    python3 lerobot_to_custom_format.py <path_to_input.json> <path_to_output.yaml> <leader|follower>

Example:
    python3 lerobot_to_custom_format.py ./my_awesome_follower_arm.json ../config/motor_calibration.yaml follower
"""

import json
import os
import sys

import yaml


def convert_config(input_file: str, output_file: str, prefix: str) -> None:
    """
    Read JSON configuration, transform joint names, and write to YAML.

    Args:
        input_file: Path to the input JSON file.
        output_file: Path to the output YAML file (without prefix).
        prefix: Prefix for the output filename ('leader' or 'follower').

    Raises:
        SystemExit: On file errors or conversion issues.
    """
    # Mapping from JSON keys to output YAML joint names
    joint_mapping = {
        "shoulder_pan": "shoulder_pan_joint",
        "shoulder_lift": "shoulder_lift_joint",
        "elbow_flex": "elbow_flex_joint",
        "wrist_flex": "wrist_flex_joint",
        "wrist_roll": "wrist_roll_joint",
        "gripper": "gripper_jaw_joint",
    }

    # Read JSON
    try:
        with open(input_file, 'r', encoding='utf-8') as f:
            data = json.load(f)
    except json.JSONDecodeError as e:
        print(f"JSON parse error: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"Failed to read input file: {e}")
        sys.exit(1)

    # Transform data
    output_data = {}
    for json_key, yaml_key in joint_mapping.items():
        if json_key not in data:
            print(f"Warning: key '{json_key}' not found in JSON, skipping.")
            continue

        joint_data = data[json_key]
        output_data[yaml_key] = {
            "drive_mode": joint_data.get("drive_mode", 0),
            "range_min": joint_data.get("range_min"),
            "range_max": joint_data.get("range_max"),
        }

        # Warn if required fields are missing
        if output_data[yaml_key]["range_min"] is None or output_data[yaml_key]["range_max"] is None:
            print(f"Warning: range_min or range_max missing for '{json_key}', written as None.")

    # Create output directory if needed
    output_dir = os.path.dirname(output_file)
    if output_dir and not os.path.exists(output_dir):
        try:
            os.makedirs(output_dir)
        except OSError as e:
            print(f"Failed to create output directory '{output_dir}': {e}")
            sys.exit(1)

    # Form the final output path: prefix is added to the filename, not to the whole path
    output_basename = os.path.basename(output_file)
    final_output = os.path.join(output_dir, f"{prefix}_{output_basename}")

    # Write YAML
    try:
        with open(final_output, 'w', encoding='utf-8') as f:
            yaml.dump(
                output_data,
                f,
                default_flow_style=False,
                sort_keys=False,
                allow_unicode=True,
                indent=2
            )
    except Exception as e:
        print(f"Failed to write YAML file: {e}")
        sys.exit(1)

    print("Conversion completed successfully.")
    print(f"Input file: {input_file}")
    print(f"Output file: {final_output}")


def main():
    """Parse command line arguments and run conversion."""
    if len(sys.argv) != 4:
        print("Error: Please specify input JSON, output YAML paths, and prefix.")
        print(f"Usage: {sys.argv[0]} <input_json> <output_yaml> <leader|follower>")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]
    prefix = sys.argv[3]

    if prefix not in ("leader", "follower"):
        print("Error: prefix must be 'leader' or 'follower'")
        sys.exit(1)

    if not os.path.isfile(input_file):
        print(f"Error: Input file not found: {input_file}")
        sys.exit(1)

    convert_config(input_file, output_file, prefix)


if __name__ == "__main__":
    main()