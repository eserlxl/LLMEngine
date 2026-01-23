#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Şevket Eser KUBALI
# Run a binary with AddressSanitizer and LeakSanitizer options
# Usage: ./scripts/run-asan.sh <command> [args...]

export ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:strict_string_checks=1:check_initialization_order=1:detect_stack_use_after_return=1:print_stats=1
export LSAN_OPTIONS=suppressions=lsan.supp:print_suppressions=0:fast_unwind_on_malloc=0
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1

# Run the test or binary
"$@"
