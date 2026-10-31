---
name: code-refactor
description: >-
  Find code duplication, simplify implementations,
  and improve C++ architecture without changing behavior.
---

# Refactoring workflow

1. Scan the requested files for:
   - Duplicated code blocks
   - Repeated conditions and branches
   - Repeated initialization and cleanup
   - Unnecessary abstractions
   - Excessive boilerplate

2. Identify opportunities to:
   - Extract common functions
   - Reuse existing utilities
   - Simplify conditional logic
   - Apply RAII where appropriate
   - Consolidate similar implementations

3. Preserve:
   - Public interfaces unless approved
   - Runtime behavior
   - Memory ownership semantics
   - Performance-sensitive code paths

4. Before modifying code:
   - Explain proposed changes
   - Estimate complexity and performance impact

5. After modifying code:
   - Build affected targets
   - Run relevant tests
   - Review the resulting diff
   - Report any unverified behavior