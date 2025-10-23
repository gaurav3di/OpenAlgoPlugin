# Build Guide - OpenAlgo AmiBroker Plugin

Complete guide for building the OpenAlgo AmiBroker Plugin from source.

## Table of Contents
1. [Prerequisites](#prerequisites)
2. [Environment Setup](#environment-setup)
3. [Building the Plugin](#building-the-plugin)
4. [Testing](#testing)
5. [Deployment](#deployment)
6. [Troubleshooting Build Issues](#troubleshooting-build-issues)
7. [Development Workflow](#development-workflow)

## Prerequisites

### Required Software

#### 1. Visual Studio
- **Version**: Visual Studio 2019 or later (2022 recommended)
- **Edition**: Community, Professional, or Enterprise
- **Workloads Required**:
  - Desktop development with C++
  - Windows 10/11 SDK
  - C++ MFC for latest build tools (x86 & x64)

#### 2. Windows SDK
- **Version**: Windows 10 SDK (10.0.19041.0) or later
- Automatically installed with Visual Studio C++ workload

#### 3. Am iBroker SDK
- **Location**: Included in `ADK` folder
- **Files**: `Plugin.h`, `Plugin_Legacy.h`
- No separate installation needed

### Optional but Recommended

- **Git**: For version control
- **GitHub Desktop**: For easier Git operations
- **VS Code**: For quick edits and documentation
- **WinMerge** or **Beyond Compare**: For comparing code changes

## Environment Setup

### 1. Clone Repository

```bash
git clone https://github.com/marketcalls/OpenAlgo-Amibroker-Plugin.git
cd OpenAlgo-Amibroker-Plugin/OpenAlgoPlugin
```

### 2. Open Solution

1. Launch Visual Studio
2. Open `OpenAlgoPlugin.sln`
3. Wait for solution to load and restore NuGet packages (if any)

### 3. Verify Configuration

Check these settings in Visual Studio:

**Project Properties → General**
- **Configuration Type**: Dynamic Library (.dll)
- **Target Name**: OpenAlgo
- **Platform Toolset**: Visual Studio 2019 (v142) or later
- **Windows SDK Version**: 10.0 (latest installed)
- **Character Set**: Use Unicode

**Project Properties → C/C++ → General**
- **Additional Include Directories**:
  - `$(ProjectDir)`
  - `$(ProjectDir)ADK\Include`

**Project Properties → C/C++ → Precompiled Headers**
- **Precompiled Header**: Use (/Yu)
- **Precompiled Header File**: StdAfx.h

**Project Properties → Linker → General**
- **Additional Library Directories**: `$(ProjectDir)ADK\Lib`

## Building the Plugin

### Quick Build

#### Debug Build (for development)
```
1. Select "Debug" configuration from toolbar
2. Select "x64" platform
3. Press F7 or Build → Build Solution
```

Output: `x64\Debug\OpenAlgo.dll`

#### Release Build (for distribution)
```
1. Select "Release" configuration from toolbar
2. Select "x64" platform
3. Press F7 or Build → Build Solution
```

Output: `x64\Release\OpenAlgo.dll`

### Build Configurations

#### Debug Configuration
- **Purpose**: Development and debugging
- **Optimizations**: Disabled (/Od)
- **Debug Info**: Full (/Zi)
- **Runtime Library**: Multi-threaded Debug DLL (/MDd)
- **Size**: Larger (~2-3 MB)
- **Performance**: Slower

**Use Debug build when**:
- Developing new features
- Debugging issues
- Testing changes
- Learning codebase

#### Release Configuration
- **Purpose**: Production deployment
- **Optimizations**: Maximum (/O2)
- **Debug Info**: None or minimal
- **Runtime Library**: Multi-threaded DLL (/MD)
- **Size**: Smaller (~500 KB)
- **Performance**: Optimal

**Use Release build when**:
- Creating distributable version
- Performance testing
- Final testing before release
- Deploying to users

### Build from Command Line

#### Using Developer Command Prompt

Open **Developer Command Prompt for VS 2019** (or later):

```batch
cd C:\Users\Admin1\source\repos\OpenAlgoPlugin\OpenAlgoPlugin
msbuild OpenAlgoPlugin.sln /p:Configuration=Release /p:Platform=x64
```

#### Using MSBuild directly

```batch
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe" ^
  OpenAlgoPlugin.sln ^
  /p:Configuration=Release ^
  /p:Platform=x64 ^
  /m
```

Parameters:
- `/p:Configuration=Release` - Build in Release mode
- `/p:Platform=x64` - Target 64-bit platform
- `/m` - Use multiple CPU cores

### Build All Configurations

To build both Debug and Release:

```batch
msbuild OpenAlgoPlugin.sln /p:Configuration=Debug /p:Platform=x64
msbuild OpenAlgoPlugin.sln /p:Configuration=Release /p:Platform=x64
```

## Testing

### Unit Testing

Currently, unit tests are minimal. To add tests:

1. Create a test project (Google Test or CTest)
2. Add test cases for critical functions
3. Run tests before committing changes

### Integration Testing

#### Test with AmiBroker

1. **Copy DLL to AmiBroker**:
   ```batch
   copy /Y "x64\Release\OpenAlgo.dll" "C:\Program Files\AmiBroker\Plugins\"
   ```

2. **Restart AmiBroker**

3. **Open Configuration**:
   - File → Database Settings → Configure
   - Verify plugin appears in list

4. **Test Connection**:
   - Enter server details
   - Click "Test Connection"
   - Click "Test WebSocket"
   - Both should show success

5. **Test Historical Data**:
   - Add a symbol (e.g., RELIANCE-NSE)
   - Verify chart loads with data
   - Check for any errors in AmiBroker log

6. **Test Real-time Data**:
   - Open quote window
   - Verify LTP updates
   - Check status LED color

### Debugging in AmiBroker

#### Attach Debugger

1. Build Debug configuration
2. Copy DLL to AmiBroker Plugins folder
3. In Visual Studio: **Debug → Attach to Process**
4. Select `Broker.exe` (AmiBroker)
5. Click **Attach**
6. Set breakpoints in plugin code
7. Trigger plugin functionality in AmiBroker

#### Debug Output

Add debug logging:

```cpp
#ifdef _DEBUG
    OutputDebugString(_T("Debug message here\n"));
#endif
```

View output in Visual Studio **Output** window or use [DebugView](https://learn.microsoft.com/en-us/sysinternals/downloads/debugview).

## Deployment

### Creating Release Package

#### Step 1: Build Release Version

```batch
msbuild OpenAlgoPlugin.sln /p:Configuration=Release /p:Platform=x64
```

#### Step 2: Gather Files

Create deployment folder structure:

```
OpenAlgo-Plugin-v1.0.0/
├── OpenAlgo.dll           (from x64\Release\)
├── README.md              (user guide)
├── LICENSE                (license file)
└── docs/
    ├── INSTALLATION.md
    └── TROUBLESHOOTING.md
```

#### Step 3: Create ZIP Archive

```batch
cd x64\Release
powershell Compress-Archive -Path OpenAlgo.dll,README.md,LICENSE -DestinationPath OpenAlgo-Plugin-v1.0.0.zip
```

#### Step 4: Test Package

1. Extract ZIP to clean folder
2. Copy DLL to fresh AmiBroker installation
3. Test all functionality
4. Verify no errors or missing dependencies

### Distributing Updates

#### Version Numbering

Follow [Semantic Versioning](https://semver.org/):
- **MAJOR.MINOR.PATCH** (e.g., 1.2.3)
- Increment MAJOR for breaking changes
- Increment MINOR for new features
- Increment PATCH for bug fixes

#### Update Version in Code

Edit `Plugin.cpp`:

```cpp
#define PLUGIN_VERSION 10003  // Version 1.0.3
```

Format: `MAJOR*10000 + MINOR*100 + PATCH`

#### Create GitHub Release

1. Tag version: `git tag v1.0.3`
2. Push tag: `git push origin v1.0.3`
3. Create Release on GitHub
4. Upload ZIP file
5. Write release notes (see CHANGELOG.md)

## Troubleshooting Build Issues

### Common Build Errors

#### Error: "Cannot open include file 'afxwin.h'"

**Cause**: MFC libraries not installed

**Solution**:
1. Open Visual Studio Installer
2. Modify installation
3. Check "C++ MFC for latest build tools"
4. Click Modify and wait for installation

#### Error: "LNK1120: unresolved externals"

**Cause**: Missing library or incorrect linker settings

**Solution**:
1. Check **Linker → Input → Additional Dependencies**
2. Verify all required libraries are listed:
   - `ws2_32.lib` (WinSock)
   - `wininet.lib` (WinInet)
3. Clean and rebuild solution

#### Error: "RC1015: cannot open include file 'afxres.h'"

**Cause**: MFC resources not found

**Solution**:
1. Verify MFC is installed
2. Check **Include Directories** contains SDK paths
3. Rebuild solution

#### Error: "Cannot find or open PDB file"

**Cause**: Debug symbols not found (warning, not critical)

**Solution**:
- This is a warning, can be ignored
- Or download symbols from Microsoft Symbol Server
- Or disable symbol loading for system DLLs

### Clean Build

If build is corrupted:

```batch
# Delete all build artifacts
rmdir /S /Q x64
rmdir /S /Q .vs

# Rebuild solution
msbuild OpenAlgoPlugin.sln /t:Rebuild /p:Configuration=Release /p:Platform=x64
```

In Visual Studio:
- **Build → Clean Solution**
- **Build → Rebuild Solution**

## Development Workflow

### Best Practices

#### 1. Branch Strategy

```bash
# Create feature branch
git checkout -b feature/new-feature

# Make changes and commit
git add .
git commit -m "Add new feature"

# Push to GitHub
git push origin feature/new-feature

# Create Pull Request on GitHub
```

#### 2. Before Committing

- [ ] Build succeeds in both Debug and Release
- [ ] No compiler warnings
- [ ] Code is formatted consistently
- [ ] Comments added for complex logic
- [ ] Tested in AmiBroker
- [ ] Documentation updated if needed

#### 3. Commit Message Format

Use clear, descriptive commit messages:

```
<type>: <subject>

<body>

<footer>
```

**Types**:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting)
- `refactor`: Code refactoring
- `test`: Adding tests
- `chore`: Build or tooling changes

**Example**:
```
fix: Normalize timestamps for 1-minute bars

- Added timestamp normalization in Plugin.cpp:708-717
- Prevents freak candles during live updates
- Matches approach used for daily bars

Fixes #42
```

### Code Style Guidelines

#### Naming Conventions

```cpp
// Classes: PascalCase
class ConfigDialog { };

// Functions: PascalCase
void GetHistoricalData();

// Variables: camelCase
int quoteIndex = 0;

// Global variables: g_ prefix
CString g_oApiKey;

// Constants: UPPER_CASE
#define MAX_BUFFER_SIZE 4096

// Member variables: m_ prefix
CString m_sServerUrl;
```

#### Formatting

- **Indentation**: Tabs (4 spaces)
- **Braces**: K&R style (opening brace on same line)
- **Line length**: Max 120 characters
- **Comments**: Use // for single-line, /* */ for multi-line

#### Example:

```cpp
// Good: Clear function with proper formatting
void ProcessQuoteData(const CString& symbol, float price)
{
    if (price > 0)
    {
        // Update cache with new price
        UpdateQuoteCache(symbol, price);
    }
    else
    {
        // Log error for invalid price
        LogError(_T("Invalid price received"));
    }
}
```

### Adding New Features

#### Template for New Feature

1. **Plan**: Write design document
2. **Branch**: Create feature branch
3. **Implement**: Write code with comments
4. **Test**: Test in Debug mode
5. **Review**: Self-review code
6. **Document**: Update documentation
7. **Build**: Build Release version
8. **Test**: Test Release build
9. **Commit**: Commit with clear message
10. **PR**: Create Pull Request

## Continuous Integration (Future)

### GitHub Actions (Planned)

Create `.github/workflows/build.yml`:

```yaml
name: Build Plugin

on: [push, pull_request]

jobs:
  build:
    runs-on: windows-latest

    steps:
    - uses: actions/checkout@v2

    - name: Add MSBuild to PATH
      uses: microsoft/setup-msbuild@v1

    - name: Build
      run: msbuild OpenAlgoPlugin.sln /p:Configuration=Release /p:Platform=x64

    - name: Upload artifact
      uses: actions/upload-artifact@v2
      with:
        name: OpenAlgo.dll
        path: x64/Release/OpenAlgo.dll
```

## Resources

### Documentation
- [Visual Studio Docs](https://docs.microsoft.com/en-us/visualstudio/)
- [MSBuild Reference](https://docs.microsoft.com/en-us/visualstudio/msbuild/)
- [AmiBroker ADK](https://www.amibroker.com/guide/h_adk.html)
- [MFC Documentation](https://docs.microsoft.com/en-us/cpp/mfc/)

### Tools
- [Visual Studio](https://visualstudio.microsoft.com/)
- [Git](https://git-scm.com/)
- [DebugView](https://learn.microsoft.com/en-us/sysinternals/downloads/debugview)
- [Dependency Walker](http://www.dependencywalker.com/)

### Community
- GitHub Issues: Bug reports and feature requests
- OpenAlgo Forum: General discussion
- Discord: Real-time chat

---

*Happy Building! If you encounter issues not covered here, please open an issue on GitHub.*
