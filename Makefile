# ProGearSDK Top-Level Makefile
#
# Orchestrates building the SDK and demo projects.
#
# Targets:
#   all          - Build SDK and all demos
#   sdk          - Build only the SDK library
#   showcase     - Build SDK and showcase demo
#   template     - Build SDK and template demo
#   clean        - Clean all build artifacts
#   format       - Format all source files (SDK + demos)
#   format-check - Check formatting for all source files
#   lint         - Run static analysis on all source files
#   check        - Run all checks (format-check + lint)

.PHONY: all sdk showcase template clean format format-check lint check help

# Default target
all: sdk showcase template
	@echo ""
	@echo "All builds complete!"

# Build SDK library
sdk:
	@echo "=== Building SDK ==="
	@$(MAKE) -C sdk all

# Build showcase demo (depends on SDK)
showcase: sdk
	@echo "=== Building Showcase Demo ==="
	@$(MAKE) -C demos/showcase all

# Build template demo (depends on SDK)
template: sdk
	@echo "=== Building Template Demo ==="
	@$(MAKE) -C demos/template all

# Clean everything
clean:
	@echo "Cleaning all build artifacts..."
	@$(MAKE) -C sdk clean
	@$(MAKE) -C demos/showcase clean
	@$(MAKE) -C demos/template clean
	@echo "Clean complete."

# === Code Quality Targets ===

# Format all source files
format:
	@echo "=== Formatting All Source Files ==="
	@$(MAKE) -C sdk format
	@$(MAKE) -C demos/showcase format
	@echo "All formatting complete."

# Check formatting (fails if changes needed)
format-check:
	@echo "=== Checking Code Formatting ==="
	@$(MAKE) -C sdk format-check
	@$(MAKE) -C demos/showcase format-check
	@echo "All formatting checks passed."

# Static analysis
lint:
	@echo "=== Running Static Analysis ==="
	@$(MAKE) -C sdk lint
	@$(MAKE) -C demos/showcase lint
	@echo "All static analysis passed."

# Run all checks
check: format-check lint
	@echo ""
	@echo "All checks passed!"

# Help
help:
	@echo "ProGearSDK Build System"
	@echo ""
	@echo "Build targets:"
	@echo "  all       - Build SDK and all demos (default)"
	@echo "  sdk       - Build only the SDK library"
	@echo "  showcase  - Build SDK and showcase demo"
	@echo "  template  - Build SDK and template demo"
	@echo "  clean     - Clean all build artifacts"
	@echo ""
	@echo "Code quality targets:"
	@echo "  format       - Auto-format all source files"
	@echo "  format-check - Check formatting (fails if changes needed)"
	@echo "  lint         - Run static analysis with cppcheck"
	@echo "  check        - Run all checks (format-check + lint)"
	@echo ""
	@echo "Run demos in MAME:"
	@echo "  cd demos/showcase && make mame"
	@echo "  cd demos/template && make mame"
