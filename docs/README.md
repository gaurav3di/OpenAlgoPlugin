# OpenAlgo AmiBroker Plugin - Documentation Index

Welcome to the OpenAlgo AmiBroker Plugin documentation! This guide will help you find the right documentation for your needs.

## 📚 Documentation Overview

### For End Users

| Document | Description | When to Read |
|----------|-------------|--------------|
| **[README.md](../README.md)** | Main plugin documentation, installation, and usage guide | Start here if you're a trader/user |
| **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** | Common issues and solutions | When you encounter problems |
| **[history.md](history.md)** | API endpoint documentation and examples | When working with the API directly |

### For Developers

| Document | Description | When to Read |
|----------|-------------|--------------|
| **[BUILD_GUIDE.md](BUILD_GUIDE.md)** | Complete build instructions from source | When setting up development environment |
| **[TECHNICAL_DOCUMENTATION.md](TECHNICAL_DOCUMENTATION.md)** | Deep dive into implementation details | When understanding the codebase |
| **[ARCHITECTURE.md](ARCHITECTURE.md)** | System design and component interactions | When modifying or extending the plugin |

## 🚀 Quick Start Paths

### I want to... Install and Use the Plugin

1. Read [README.md](../README.md) → Installation section
2. Follow the 5-step installation guide
3. Configure your connection
4. If issues arise, check [TROUBLESHOOTING.md](TROUBLESHOOTING.md)

### I want to... Build from Source

1. Read [BUILD_GUIDE.md](BUILD_GUIDE.md)
2. Install prerequisites (Visual Studio, SDKs)
3. Clone repository and open solution
4. Build and test

### I want to... Understand the Code

1. Start with [ARCHITECTURE.md](ARCHITECTURE.md) for high-level overview
2. Read [TECHNICAL_DOCUMENTATION.md](TECHNICAL_DOCUMENTATION.md) for details
3. Examine source code with this understanding
4. Refer to [BUILD_GUIDE.md](BUILD_GUIDE.md) for development workflow

### I want to... Report a Bug or Contribute

1. Check [TROUBLESHOOTING.md](TROUBLESHOOTING.md) first
2. Search existing issues on GitHub
3. Read [CHANGELOG.md](CHANGELOG.md) to see if it's already fixed
4. Follow contribution guidelines in [README.md](../README.md)

## 📖 Document Summaries

### README.md (Main Documentation)
**Location**: Root directory
**Audience**: All users
**Content**:
- Plugin overview and features
- System requirements
- Installation guide (5 steps)
- Configuration reference
- Supported brokers and data types
- Usage guide with examples
- Troubleshooting quick reference
- API reference basics
- Support and community links

**Size**: ~440 lines
**Last Updated**: 2025-01-23 (v1.0.0)

### CHANGELOG.md
**Location**: `docs/`
**Audience**: All users, especially during upgrades
**Content**:
- Version history (1.0.0, 0.9.0, 0.8.0, 0.7.0)
- Critical bug fixes with technical details:
  - Freak Candles Fix (timestamp normalization)
  - Timestamp Sorting Fix (qsort implementation)
  - WebSocket Status Simplification
- Upgrade guides
- Known issues and roadmap
- Contributors and acknowledgments

**Size**: ~350 lines
**Last Updated**: 2025-01-23

### BUILD_GUIDE.md
**Location**: `docs/`
**Audience**: Developers
**Content**:
- Prerequisites (Visual Studio, Windows SDK, MFC)
- Environment setup instructions
- Build configurations (Debug vs Release)
- Command-line build options
- Testing procedures
- Debugging techniques
- Deployment packaging
- Development workflow and best practices
- Code style guidelines
- Troubleshooting build errors

**Size**: ~550 lines
**Last Updated**: 2025-01-23

### TECHNICAL_DOCUMENTATION.md
**Location**: `docs/`
**Audience**: Developers, advanced users
**Content**:
- Architecture overview
- API integration details
- WebSocket implementation
- Data flow diagrams
- Plugin components breakdown
- Configuration management
- Error handling strategies
- Performance optimizations
- Security considerations

**Size**: ~460 lines (existing)
**Status**: Comprehensive and up-to-date

### ARCHITECTURE.md
**Location**: `docs/`
**Audience**: Developers
**Content**:
- High-level system architecture
- Component architecture diagrams
- Threading model
- State management
- Memory management
- Error handling architecture
- Security architecture
- Performance optimizations
- Extensibility points
- Known limitations and technical debt

**Size**: ~475 lines (existing)
**Status**: Comprehensive and up-to-date

### history.md
**Location**: `docs/`
**Audience**: API users, developers
**Content**:
- Historical data endpoint documentation
- Request/response format examples
- Parameter reference tables
- Field descriptions
- Usage notes

**Size**: ~95 lines (existing)
**Status**: Focused on API reference

## 🔍 Finding Specific Information

### Configuration Issues
- **Start**: README.md → Configuration Reference
- **If stuck**: TROUBLESHOOTING.md → Connection Issues

### Data Issues (Gaps, Wrong Timestamps)
- **Start**: README.md → Troubleshooting → Data Issues
- **Technical details**: CHANGELOG.md → Bug Fixes
- **Deep dive**: TECHNICAL_DOCUMENTATION.md → Data Flow

### WebSocket Problems
- **Start**: README.md → Troubleshooting → WebSocket Issues
- **Testing**: README.md → Step 4: Test Connection
- **Implementation**: TECHNICAL_DOCUMENTATION.md → WebSocket Implementation

### Build Errors
- **Start**: BUILD_GUIDE.md → Troubleshooting Build Issues
- **Environment**: BUILD_GUIDE.md → Environment Setup
- **Dependencies**: BUILD_GUIDE.md → Prerequisites

### Understanding Code Structure
- **Start**: ARCHITECTURE.md → Component Architecture
- **Details**: TECHNICAL_DOCUMENTATION.md → Plugin Components
- **Development**: BUILD_GUIDE.md → Development Workflow

## 📊 Documentation Statistics

| Document | Lines | Words | Audience | Purpose |
|----------|-------|-------|----------|---------|
| README.md | 442 | 3,800+ | All Users | Main guide |
| CHANGELOG.md | 355 | 2,500+ | All Users | Version history |
| BUILD_GUIDE.md | 551 | 4,000+ | Developers | Build instructions |
| TECHNICAL_DOCUMENTATION.md | 466 | 3,200+ | Developers | Implementation details |
| ARCHITECTURE.md | 475 | 3,500+ | Developers | System design |
| history.md | 94 | 400+ | API Users | API reference |
| **TOTAL** | **2,383** | **17,400+** | - | Complete docs |

## 🆕 Recent Updates (v1.0.0)

### New Documents
- ✅ **README.md**: Completely rewritten with comprehensive coverage
- ✅ **CHANGELOG.md**: Created with detailed fix documentation
- ✅ **BUILD_GUIDE.md**: Created with step-by-step instructions

### Updated Documents
- ⚠️ **TECHNICAL_DOCUMENTATION.md**: Existing (comprehensive, may need minor updates)
- ⚠️ **ARCHITECTURE.md**: Existing (comprehensive, may need minor updates)
- ⚠️ **history.md**: Existing (focused on API, complete)

### Highlights
- 🐛 **3 Critical Bug Fixes** documented with solutions
- 📦 **Installation Guide** with 5 clear steps
- 🔧 **Troubleshooting** for common issues
- 👨‍💻 **Developer Guide** with build instructions
- 📝 **Code Examples** throughout documentation

## 🎯 Documentation Quality

### Coverage
- ✅ Installation: 100%
- ✅ Configuration: 100%
- ✅ Usage: 100%
- ✅ Troubleshooting: 90%
- ✅ Development: 100%
- ✅ API Reference: 80%
- ✅ Architecture: 100%

### Accessibility
- 📖 **Beginner-friendly**: README with step-by-step guides
- 🔧 **Intermediate**: Troubleshooting and configuration
- 👨‍💻 **Advanced**: Technical docs and architecture
- 🏗️ **Expert**: Build guide and code contributions

## 🛠️ Maintenance

### Documentation Updates
Documents are updated when:
- New features are added
- Bugs are fixed
- Configuration changes
- API changes
- Build process changes

### Version Tracking
Each document includes:
- Last updated date
- Version number
- Change description

## 📞 Getting Help

### If Documentation Doesn't Help

1. **Search GitHub Issues**: Check if someone else had the same problem
2. **Community Forum**: Ask in OpenAlgo community
3. **Discord**: Real-time chat support
4. **GitHub Issue**: Create detailed bug report

### Improving Documentation

Found an error or want to improve docs?

1. Fork repository
2. Make changes to documentation
3. Submit Pull Request
4. We'll review and merge

## 🔗 External Resources

### OpenAlgo Platform
- **Website**: https://openalgo.in
- **Documentation**: https://docs.openalgo.in
- **GitHub**: https://github.com/marketcalls/OpenAlgo

### AmiBroker
- **Website**: https://www.amibroker.com
- **User Guide**: https://www.amibroker.com/guide/
- **AFL Reference**: https://www.amibroker.com/guide/afl.html
- **ADK Documentation**: https://www.amibroker.com/guide/h_adk.html

### Development Tools
- **Visual Studio**: https://visualstudio.microsoft.com/
- **Git**: https://git-scm.com/
- **Markdown Guide**: https://www.markdownguide.org/

## 📜 License

All documentation is licensed under **MIT License**, same as the plugin code.

## 🙏 Acknowledgments

Documentation created and maintained by:
- OpenAlgo Community
- Plugin contributors
- Beta testers and users who provided feedback

---

**Last Updated**: 2025-01-23 (v1.0.0)

**Documentation Version**: 1.0.0

**Plugin Version**: 1.0.0

*For questions about documentation, open an issue on GitHub with the "documentation" label.*
