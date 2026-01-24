# ProGearSDK Top-Level Makefile
#
# Orchestrates building the HAL, SDK, and demo projects.
#
# Targets:
#   all          - Build HAL, SDK, and all demos
#   hal          - Build only the HAL library (libneogeo.a)
#   sdk          - Build HAL and SDK library (libprogearsdk.a)
#   showcase     - Build HAL, SDK, and showcase demo
#   template     - Build HAL, SDK, and template demo
#   hal-template - Build HAL and hal-template demo (HAL-only example)
#   clean        - Clean all build artifacts
#   docs         - Generate API documentation with Doxygen
#   format       - Format all source files (HAL + SDK + demos)
#   format-check - Check formatting for all source files
#   lint         - Run static analysis on all source files
#   check        - Run all checks (format-check + lint)

.PHONY: all hal sdk showcase template hal-template clean docs format format-check lint check help

# Default target
all: sdk showcase template hal-template
	@echo ""
	@echo "All builds complete!"

# Build HAL library
hal:
	@echo "=== Building HAL ==="
	@$(MAKE) -C hal all

# Build SDK library (depends on HAL)
sdk: hal
	@echo "=== Building SDK ==="
	@$(MAKE) -C sdk all

# Build showcase demo (depends on SDK which depends on HAL)
showcase: sdk
	@echo "=== Building Showcase Demo ==="
	@$(MAKE) -C demos/showcase all

# Build template demo (depends on SDK which depends on HAL)
template: sdk
	@echo "=== Building Template Demo ==="
	@$(MAKE) -C demos/template all

# Build HAL-only template (depends only on HAL, not SDK)
hal-template: hal
	@echo "=== Building HAL Template Demo ==="
	@$(MAKE) -C demos/hal-template all

# Clean everything
clean:
	@echo "Cleaning all build artifacts..."
	@$(MAKE) -C hal clean
	@$(MAKE) -C sdk clean
	@$(MAKE) -C demos/showcase clean
	@$(MAKE) -C demos/template clean
	@$(MAKE) -C demos/hal-template clean
	@echo "Clean complete."

# === Documentation ===

# Generate API documentation with Doxygen
docs:
	@command -v doxygen >/dev/null 2>&1 || { echo "Error: doxygen not found. Install with: brew install doxygen"; exit 1; }
	@echo "Generating API documentation..."
	@doxygen Doxyfile
	@echo "Documentation generated in docs/api/"

# === Code Quality Targets ===

# Format all source files
format:
	@echo "=== Formatting All Source Files ==="
	@$(MAKE) -C hal format
	@$(MAKE) -C sdk format
	@$(MAKE) -C demos/showcase format
	@echo "All formatting complete."

# Check formatting (fails if changes needed)
format-check:
	@echo "=== Checking Code Formatting ==="
	@$(MAKE) -C hal format-check
	@$(MAKE) -C sdk format-check
	@$(MAKE) -C demos/showcase format-check
	@echo "All formatting checks passed."

# Static analysis
lint:
	@echo "=== Running Static Analysis ==="
	@$(MAKE) -C hal lint
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
	@echo "  all          - Build HAL, SDK, and all demos (default)"
	@echo "  hal          - Build only the HAL library (libneogeo.a)"
	@echo "  sdk          - Build HAL and SDK library (libprogearsdk.a)"
	@echo "  showcase     - Build SDK and showcase demo"
	@echo "  template     - Build SDK and template demo"
	@echo "  hal-template - Build HAL-only template (no SDK)"
	@echo "  clean        - Clean all build artifacts"
	@echo "  docs         - Generate API documentation"
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
	@echo "  cd demos/hal-template && make mame"
