# ProGearSDK Top-Level Makefile
#
# Orchestrates building the Core, HAL, ProGear, and demo projects.
#
# Targets:
#   all          - Build Core, HAL, ProGear, and all demos
#   core         - Build only the Core library (libneogeo_core.a)
#   hal          - Build Core and HAL library (libneogeo.a)
#   progear      - Build Core, HAL, and ProGear library (libprogearsdk.a)
#   showcase     - Build ProGear and showcase demo
#   template     - Build ProGear and template demo
#   hal-template - Build HAL and hal-template demo (HAL-only example)
#   clean        - Clean all build artifacts
#   docs         - Generate API documentation with Doxygen
#   format       - Format all source files (Core + HAL + ProGear + demos)
#   format-check - Check formatting for all source files
#   lint         - Run static analysis on all source files
#   check        - Run all checks (format-check + lint)

.PHONY: all core hal progear showcase template hal-template clean docs format format-check lint check help

# Default target
all: progear showcase template hal-template
	@echo ""
	@echo "All builds complete!"

# Build Core library
core:
	@echo "=== Building Core ==="
	@$(MAKE) -C core all

# Build HAL library (depends on Core)
hal: core
	@echo "=== Building HAL ==="
	@$(MAKE) -C hal all

# Build ProGear library (depends on HAL which depends on Core)
progear: hal
	@echo "=== Building ProGear ==="
	@$(MAKE) -C progear all

# Build showcase demo (depends on ProGear which depends on HAL)
showcase: progear
	@echo "=== Building Showcase Demo ==="
	@$(MAKE) -C demos/showcase all

# Build template demo (depends on ProGear which depends on HAL)
template: progear
	@echo "=== Building Template Demo ==="
	@$(MAKE) -C demos/template all

# Build HAL-only template (depends only on HAL, not ProGear)
hal-template: hal
	@echo "=== Building HAL Template Demo ==="
	@$(MAKE) -C demos/hal-template all

# Clean everything
clean:
	@echo "Cleaning all build artifacts..."
	@$(MAKE) -C core clean
	@$(MAKE) -C hal clean
	@$(MAKE) -C progear clean
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
	@$(MAKE) -C core format
	@$(MAKE) -C hal format
	@$(MAKE) -C progear format
	@$(MAKE) -C demos/showcase format
	@echo "All formatting complete."

# Check formatting (fails if changes needed)
format-check:
	@echo "=== Checking Code Formatting ==="
	@$(MAKE) -C core format-check
	@$(MAKE) -C hal format-check
	@$(MAKE) -C progear format-check
	@$(MAKE) -C demos/showcase format-check
	@echo "All formatting checks passed."

# Static analysis
lint:
	@echo "=== Running Static Analysis ==="
	@$(MAKE) -C core lint
	@$(MAKE) -C hal lint
	@$(MAKE) -C progear lint
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
	@echo "  all          - Build Core, HAL, ProGear, and all demos (default)"
	@echo "  core         - Build only the Core library (libneogeo_core.a)"
	@echo "  hal          - Build Core and HAL library (libneogeo.a)"
	@echo "  progear      - Build Core, HAL, and ProGear library (libprogearsdk.a)"
	@echo "  showcase     - Build ProGear and showcase demo"
	@echo "  template     - Build ProGear and template demo"
	@echo "  hal-template - Build HAL-only template (no ProGear)"
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
