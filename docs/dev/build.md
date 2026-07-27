# Building Ieum

To build Ieum you will a minimum of:
    - [cmake] 3.24+
    - [Qt] 6.7.0+
    - [openssl] 3.0+
    - [libportal] 0.9.1+ (linux, bsd)
    - [libei] 1.3+ (linux, bsd)
    - [google_test] ^

> ^ Will be fetched if not found on the host system.

By default a build of Ieum will:
     - The GUI application `deskflow`
     - The Core application `deskflow-core`
     - Documentation if [doxygen] was found on your system
     - Tests that will be run as part of the build process

## Configuration

Ieum supports the following CMake options:

|         Option           |            Description                  |   Default Value    | Additional requirements |
:-------------------------:|:---------------------------------------:|:------------------:|:-----------------------:|
| BUILD_USER_DOCS          | Build user documentation                | DOXYGEN_FOUND      | `Doxygen` |
| BUILD_DEV_DOCS           | Build development documentation         | OFF                | `Doxygen` |
| BUILD_INSTALLER          | Build installers/packages               | ON                 | |
| BUILD_TESTS              | Build unit tests and legacy tests       | ON                 | `gtest`|
| BUILD_X11_SUPPORT        | Build X11 backend (Linux and BSD only)  | ON                 | `x11 libs`|
| BUILD_OSX_BUNDLE         | Build an app bundle (macOS only)        | ON                 | |
| ENABLE_COVERAGE          | Enable test coverage                    | OFF                | `gcov` |
| SKIP_BUILD_TESTS         | Skip running of tests at build time     | OFF                | |
| VCPKG_QT                 | Build Qt w/ vcpkg (Windows only)        | OFF                | |
| CLEAN_TRS                | Remove obsolete strings from tr files   | OFF                | |
| APPLE_CODESIGN_DEV       | Apple codesign cert ID for development  | Not set            | | 

Example cmake configuration:
`cmake -S. -Bbuild -DCMAKE_INSTALL_PREFIX=<INSTALLPREFIX>`

### Windows Configuration

 It is recommended to use vcpkg to install the dependencies. The first time you configure Ieum, all dependencies other than Qt will be built. If you don't want to use vcpkg, you must manually setup the dependencies. However, that will not be covered by this document.
 
#### Windows and Qt

 There are two ways you can install [Qt] on Windows (vcpkg or Qt online installer). The default configuration expects you to use the Qt online installer. You should not install Qt in both ways, as having both can cause some weird things to happen, like Qt getting libs from one install and plugins from the other. When switching between them, remove the previous install first.
 
##### System Qt

 1. Download and install the [Qt] online installer from their website.
 2. Add the path of Qt's cmake files to your system path (skipping this may require you provide this path to cmake via `Qt6_DIR` at configure time).
   - Often: `C:\Qt\<version>\<msvcinfo>\lib\cmake`
 3. Add the path of Qt's binary tools to your system path.
   - Often: `C:\Qt\<version>\<msvcinfo>\bin`

##### vcpkg managed Qt

 1. Add the option `-DVCPKG_QT=ON` to your cmake configuration command (i.e `cmake -S. -Bbuild -DVCPKG_QT=ON ...`) or if using an IDE, look for the option where you configure the project, have the IDE run cmake again.
 2. Once the configuration starts, you should see a lot more packages vcpkg will build. Building Qt takes a long time (potentially hours), so go find something else to do for a while.
 3. If you want to use the system Qt again, you must delete the `vcpkg.json` generated in the project root and the `build` folder and reconfigure the project from scratch.


### macOS code signing

The code signing option `APPLE_CODESIGN_DEV` is only for local development and is not intended for distributed
bundles.

Signing for local development and signing for the distribution bundle must be different because the development
entitlements are not appropriate for production. A local app bundle may use external libraries installed by
Homebrew, while a distribution bundle must contain and seal all of its dependencies.

For development signing:

1. Install Xcode
2. Go to Settings -> Accounts
3. Add your account (requires a free Apple Developer ID)
4. Manage certificates -> Add -> Apple Development
5. To get your ID, run: `security find-identity -v -p codesigning login.keychain-db`
6. Pass the ID to CMake, e.g. `-DAPPLE_CODESIGN_DEV="Apple Development: name@example.com (KLGSJHLFXY)"`
7. Configure and build
8. Verify with `codesign --verify --deep --strict --verbose=2 build/bin/Ieum.app`

For direct distribution outside the Mac App Store, an active Apple Developer Program membership and a
`Developer ID Application` certificate are required. Configure a local package build with:

```sh
cmake -S . -B build \
  -DAPPLE_CODESIGN_DISTRIBUTION="Developer ID Application: Example (TEAMID)"
cmake --build build --target package
```

CPack deploys dependencies and signs only after every installed file has reached the staged app bundle. Do not add
files to `Ieum.app` after that step; doing so invalidates its resource seal and macOS reports that the app is
damaged.

The release workflow imports the certificate into an ephemeral keychain, signs the app with Hardened Runtime,
submits the DMG with `notarytool`, and staples the accepted ticket. Configure these repository secrets:

| Secret | Value |
| --- | --- |
| `MACOS_CERTIFICATE_P12_BASE64` | Base64-encoded Developer ID Application `.p12` archive |
| `MACOS_CERTIFICATE_PASSWORD` | Password used when exporting the `.p12` archive |
| `MACOS_NOTARY_KEY_BASE64` | Base64-encoded App Store Connect Notary API `.p8` key |
| `MACOS_NOTARY_KEY_ID` | App Store Connect API key ID |
| `MACOS_NOTARY_ISSUER_ID` | App Store Connect API issuer ID |

Without these secrets, CI creates an internally consistent ad-hoc signed DMG and verifies its code seal, but
Gatekeeper cannot trust it as an identified and notarized release. macOS TCC also anchors an ad-hoc app's
Accessibility identity to a code-directory hash, which changes when the executable changes. An ad-hoc update may
therefore leave the old enabled entry in System Settings without trusting the new app. Ieum's permission dialog
offers **Reset Previous Approval** as a recovery path; it resets only
`Accessibility io.github.victoriousian.ieum` before requesting access for the current app. This is not a
replacement for distribution signing. A stable `Developer ID Application` designated requirement is what lets
macOS safely carry the authorization across ordinary updates.

## Build

After configuring you should be able to run make to build all targets.

`cmake --build build`

## Install

 To test installation run `DESTDIR=<installDIR> cmake --install build` to install into `<installDir>/<CMAKE_INSTALL_PREFIX>`

 Running `cmake --install build` will install to the `CMAKE_INSTALL_PREFIX`

## Making Ieum packages

 Ieum can generate several packages using `cpack`.
 
 To generate packages build the `package` or `package_source` target.

 Example: ` cmake --build build --target package package_source` would generate both package and package source packages.
 
 Ieum can generate several package types depending on the system.
 
 Archive-based packages should work on all platforms. On Linux deb and rpm info is set up, Flatpaks can be generated from the included file in deploy/linux and a `PKGBUILD` for Arch linux is generated in the build folder. On macOS a DMG is created with either ad-hoc or configured Developer ID signing. For Windows, WiX can be used to create an installer.
 
[Qt]:https://www.qt.io
[doxygen]:http://www.stack.nl/~dimitri/doxygen/
[cmake]:https://cmake.org/
[openssl]:https://www.openssl.org/
[google_test]:https://github.com/google/googletest
[libei]:https://gitlab.freedesktop.org/libinput/libei
[libportal]:https://github.com/flatpak/libportal
