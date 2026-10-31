# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or any later version.
# Developer entry points; production builds remain Meson / ./build.
.PHONY: check lint test verify hooks

check:
	python3 tools/maintenance/check_repo.py

lint:
	git diff --check
	git diff --cached --check
	python3 -m py_compile tools/maintenance/check_repo.py
	sh -n .githooks/pre-commit

test:
	python3 -m unittest discover -s tools/maintenance/tests -v
	python3 tests/test_profile_report.py
	python3 tests/test_session_observer.py
	python3 tests/test_session_kernel.py
	python3 tests/test_session_gc.py
	python3 tests/test_session_x_errors.py
	python3 tests/test_desktop_session_observation.py
	python3 tests/test_session_workload.py
	python3 tests/test_bsd_default_x.py
	python3 tests/test_contrast_fixture_cli.py
	python3 tests/test_native_gate.py
	python3 tests/test_ci_assets.py
	python3 tests/test_qemu_bsd.py
	python3 tests/test_ci_png.py
	python3 tests/test_go_sdk_recipe.py

verify: check lint test

hooks:
	git config core.hooksPath .githooks
