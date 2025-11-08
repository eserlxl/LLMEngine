# Pull Request

Thank you for your contribution!

## Description
Please include a summary of the change and which issue is fixed. Also include relevant motivation and context.

Fixes # (issue)

## Type of change
- [ ] Bug fix
- [ ] New feature
- [ ] Breaking change
- [ ] Documentation update
- [ ] Other (describe):

## Testing
Describe how you tested your changes.

```
Build: cmake -S . -B build
       cmake --build build --config Release -j20
Tests: ctest --test-dir build --output-on-failure
```

## Checklist
- [ ] Code builds locally with -j20
- [ ] Tests pass locally
- [ ] My code follows the style guidelines of this project
- [ ] I have performed a self-review of my code
- [ ] I have commented my code, particularly in hard-to-understand areas
- [ ] I have made corresponding changes to the documentation
- [ ] My changes generate no new warnings
- [ ] I have added tests that prove my fix is effective or that my feature works
- [ ] New and existing unit tests pass locally with my changes
- [ ] Any dependent changes have been merged and published in downstream modules
- [ ] No secrets added; environment variables used where appropriate
- [ ] Follows GPLv3 licensing of repository

