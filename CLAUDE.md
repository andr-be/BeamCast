# BeamCast Development Instructions for Claude

## Git Workflow (CRITICAL)

This project is now under version control. You MUST commit changes after completing significant work:

**When to commit:**
- After implementing a feature or fixing a bug
- After making architectural changes
- After updating documentation
- When the user explicitly asks for a commit
- At natural stopping points in multi-step work

**Commit message format:**
```
<Brief summary of changes>

<Optional detailed description>
- Bullet points for specific changes
- Reference any issues or features

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>
```

**Git commands to use:**
```bash
git add .
git commit -m "Your message here"
git push
```

**What NOT to commit:**
- Work-in-progress broken code (unless explicitly asked)
- Experimental changes that might break things
- Build artifacts (already in .gitignore)

## Spec Development (Original Instructions)

Read SPEC.md and interview the user in detail using AskUserQuestionTool about:
- Technical implementation details
- UI & UX considerations
- Concerns and tradeoffs
- Architecture decisions

Be very in-depth and continue interviewing continually until complete, then write the spec to the file.
Avoid obvious questions - dig into nuanced details.

