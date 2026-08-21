# Packaging and testing the Windows Shipping build

This POC is only successful if the **packaged Windows EXE** enforces the license. The Unreal Editor is intentionally non-fatal so you can open the project.

Unreal does not produce a Windows Shipping package from macOS. Use a **Windows** machine with Unreal Engine 5.3–5.5 (or newer 5.x).

## 1. Open the project

1. Install Unreal Engine 5.x with **C++ / Visual Studio** (VS 2022, Windows 10/11 SDK).
2. Right-click `CompanyLicensingPoc.uproject` → **Generate Visual Studio project files**.
3. Open the `.sln` or the `.uproject`. Let it compile the game module and `CompanyLicensing` plugin.
4. Confirm `LicensePublicKey.h` was generated (`python Tools/generate_keys.py` if you cloned a tree without keys).

If UBT reports an OpenSSL third-party error, the engine installation is missing the OpenSSL dependency that ships with stock UE5. Use a standard Epic launcher or source build that includes `Engine/Source/ThirdParty/OpenSSL`.

## 2. Package Shipping

In the editor:

**Platforms → Windows → Shipping → Package Project**

Or from a Developer Command Prompt (adjust engine path):

```bat
"%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
  -project="%CD%\CompanyLicensingPoc.uproject" ^
  -noP4 -platform=Win64 -clientconfig=Shipping -serverconfig=Shipping ^
  -cook -build -stage -pak -archive ^
  -archivedirectory="%CD%\Dist"
```

Output is typically:

```text
Dist\Windows\CompanyLicensingPoc.exe
```

or a folder:

```text
Dist\Windows\CompanyLicensingPoc\Binaries\Win64\CompanyLicensingPoc.exe
```

The plugin is a **Runtime** module, so its C++ is linked into this executable / its modules. There is no extra licensing EXE.

## 3. Place the license

Copy a sample file **next to the EXE** as `license.json`:

```bat
copy Tools\licenses\valid\license.json <EXE_DIR>\license.json
```

`FLicenseManager` looks at:

1. Directory of `FPlatformProcess::ExecutablePath()`
2. `FPaths::LaunchDir()`
3. `FPaths::ProjectDir()` (mainly editor)

## 4. Run tests against the EXE

Use [TEST_MATRIX.md](TEST_MATRIX.md). Example:

```bat
cd <EXE_DIR>
CompanyLicensingPoc.exe
```

For **Development** (not Shipping) grace-date simulation:

```bat
CompanyLicensingPoc.exe -LicenseFakeNow=2026-09-10T00:00:00Z
```

That switch is **compiled out** of Shipping. For Shipping grace tests, swap `grace_valid` vs `grace_expired` license files instead of faking the clock.

## 5. Confirm the logic is inside the EXE

- There is no second process to start first.
- Removing `license.json` makes **this** EXE show `License validation failed.` and quit.
- The public key lives in `LicensePublicKey.h` and is compiled into the `CompanyLicensing` module.

## 6. Development package (optional)

Package **Development** if you need `-LicenseFakeNow` on a binary that still uses packaged filesystem layout. Do not treat Development as the release configuration; Shipping remains the acceptance build.
