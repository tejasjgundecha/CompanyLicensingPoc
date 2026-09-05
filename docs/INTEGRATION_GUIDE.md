# Plugin Integration Guide: CompanyLicensing

This document provides a complete, step-by-step guide for integrating the `CompanyLicensing` plugin into an external or existing Unreal Engine 5 project.

---

## 1. Overview & Architecture

The `CompanyLicensing` plugin is completely self-contained. It performs standalone Ed25519 cryptographic license verification, machine hardware binding, anti-clock-rollback detection, and native startup error dialogs before the engine or gameplay levels load (`LoadingPhase: PostConfigInit`).

```text
Target Unreal Project
├── Plugins/
│   └── CompanyLicensing/             <-- ONLY folder needed in the Unreal Project
│       ├── CompanyLicensing.uplugin
│       └── Source/CompanyLicensing/
│           ├── CompanyLicensing.Build.cs
│           ├── Public/               <-- Headers (LicenseManager, State, Types, Validator, etc.)
│           └── Private/              <-- Implementations + Embedded Public Key
└── Source/
    └── <TargetProject>/              <-- (Optional) Game module to query FLicenseManager
```

---

## 2. Files & Folders to Copy

Copy the entire `Plugins/CompanyLicensing/` folder from this repository into your target project's root `Plugins/` directory:

| Source Path | Target Destination | Description |
|-------------|--------------------|-------------|
| `Plugins/CompanyLicensing/` | `<TargetProjectRoot>/Plugins/CompanyLicensing/` | The complete licensing plugin |

### Contents of `Plugins/CompanyLicensing/`

- **Plugin Descriptor:**
  - `CompanyLicensing.uplugin`: Configures runtime loading at `PostConfigInit`.
- **Build Configuration:**
  - `Source/CompanyLicensing/CompanyLicensing.Build.cs`: Links engine-provided `OpenSSL`, `Json`, `ApplicationCore`, and sets `OPENSSL_NO_UI`.
- **Public Headers (`Source/CompanyLicensing/Public/`):**
  - `CompanyLicensing.h`: Plugin module interface.
  - `LicenseManager.h`: Singleton managing startup validation, state retrieval, and modal dialogs.
  - `LicenseState.h`: `ELicenseState` enum, state string mappings, and localized error messages.
  - `LicenseTypes.h`: `FLicensePayload` struct, `ExpectedProjectId()`, canonical payload builder.
  - `LicenseValidator.h`: Core validation logic (dates, project ID, fingerprint, signature).
  - `MachineFingerprint.h`: Windows Motherboard UUID + CPU ID SHA-256 fingerprint generator.
  - `ClockGuard.h`: Windows Registry timestamp persistence for clock rollback detection.
  - `SignatureVerifier.h`: Raw Ed25519 signature verification via OpenSSL EVP API.
- **Private Implementations (`Source/CompanyLicensing/Private/`):**
  - `CompanyLicensing.cpp`: Plugin entrypoint triggering license enforcement at startup.
  - `LicenseManager.cpp`: License file resolution, JSON parsing, native modal UI dialogs, clean application exit.
  - `LicensePublicKey.h`: Compiled-in 32-byte Ed25519 public key.
  - `LicenseValidator.cpp`, `MachineFingerprint.cpp`, `ClockGuard.cpp`, `SignatureVerifier.cpp`.

> [!IMPORTANT]
> **Do not copy `Tools/` or private signing keys (`*.pem`) into the Unreal project repository or client builds.** Keep key generation and signing tools in a secure administrative/backend environment.

---

## 3. Required Project Changes & Customizations

### Step 1: Generate & Embed Your Ed25519 Public Key
1. In your offline/administrative environment, generate a new cryptographic keypair using `Tools/generate_keys.py`:
   ```bash
   python Tools/generate_keys.py
   ```
   This will output:
   - Private key: `Tools/keys/poc_ed25519_private.pem` (Keep strictly secure!)
   - Public key C++ array format.

2. Open `Plugins/CompanyLicensing/Source/CompanyLicensing/Private/LicensePublicKey.h` and replace `EmbeddedPublicKey` with your generated 32-byte public key:
   ```cpp
   namespace CompanyLicensing
   {
       inline const uint8 EmbeddedPublicKey[32] = {
           0x12, 0x34, 0x56, 0x78, /* ... 32 bytes ... */
       };
   }
   ```

---

### Step 2: Set Your Target Project ID
Open `Plugins/CompanyLicensing/Source/CompanyLicensing/Public/LicenseTypes.h` and configure `ExpectedProjectId()`:

```cpp
namespace CompanyLicensing
{
    /** Must match the expected identifier compiled into the application. */
    inline const TCHAR* ExpectedProjectId()
    {
        return TEXT("my-target-project-id");
    }
}
```

#### How Project ID Validation Works:
- **What it matches:** It is an arbitrary logical **Product / SKU identifier** of your choice. It does **not** need to match your Unreal `.uproject` filename, folder name, or `.exe` name.
- **Strict Equality Check:** During startup, `LicenseValidator` strictly checks:
  ```cpp
  if (Payload.ProjectId != CompanyLicensing::ExpectedProjectId())
  {
      return ELicenseState::LicenseInvalid;
  }
  ```
  The string must match the `--project-id` value passed when generating the signed license (`Tools/generate_license.py --project-id "my-target-project-id"`).
- **Purpose:** Prevents **cross-product license reuse**. If you ship multiple games/apps signed with the same private key, a customer with a valid license for *Product A* cannot reuse it to unlock *Product B*.

---

### Step 3: Enable the Plugin in Your `.uproject` File
Open `<TargetProjectRoot>/<TargetProject>.uproject` in a text editor and ensure the plugin is listed under `"Plugins"`:

```json
{
    "FileVersion": 3,
    "EngineAssociation": "5.8",
    "Category": "",
    "Description": "",
    "Modules": [
        {
            "Name": "MyTargetProject",
            "Type": "Runtime",
            "LoadingPhase": "Default"
        }
    ],
    "Plugins": [
        {
            "Name": "CompanyLicensing",
            "Enabled": true
        }
    ]
}
```

---

### Step 4: (Optional) Access License Info from Gameplay C++ / HUD
Because the plugin initializes at `PostConfigInit`, it automatically validates the license and closes the app on failure before levels load. 

If your gameplay code, HUD, or UI needs to display license details:

1. Add `"CompanyLicensing"` to `PublicDependencyModuleNames` in `<TargetProject>/Source/<TargetProject>/<TargetProject>.Build.cs`:
   ```csharp
   PublicDependencyModuleNames.AddRange(new string[] {
       "Core",
       "CoreUObject",
       "Engine",
       "InputCore",
       "CompanyLicensing"
   });
   ```

2. Query the singleton in your C++ classes:
   ```cpp
   #include "LicenseManager.h"
   #include "LicenseState.h"

   // Get validation state
   ELicenseState State = FLicenseManager::Get().GetState();

   // Get payload metadata (e.g. License ID, expiration dates)
   const FLicensePayload& Payload = FLicenseManager::Get().GetPayload();

   if (State == ELicenseState::LicenseValid)
   {
       FString LicenseId = Payload.LicenseId;
       FString ExpiryDate = Payload.SubscriptionValidUntil;
       // Update UI...
   }
   ```

---

### Step 5: Regenerate Project Files and Build
1. Right-click `<TargetProject>.uproject` and select **Generate Visual Studio project files**.
2. Open the `.sln` in Visual Studio.
3. Build the project under your desired build configuration (`Development Editor`, `Shipping`, etc.).

---

## 4. Issuing Licenses for Customers

### Step 1: Collect Customer Machine Fingerprint (Hardware Lock)
Have the customer run `Tools/collect_fingerprint.py` on their target Windows machine:
```cmd
python collect_fingerprint.py
```
This outputs a SHA-256 string derived from the motherboard UUID and CPU ID (e.g., `a1b2c3d4...`).

### Step 2: Generate Signed `license.json`
Run `Tools/generate_license.py` using your private key:
```bash
python Tools/generate_license.py \
    --private-key Tools/keys/poc_ed25519_private.pem \
    --license-id "LIC-2026-001" \
    --project-id "my-target-project-id" \
    --valid-until "2027-01-01T00:00:00Z" \
    --offline-grace-until "2027-01-15T00:00:00Z" \
    --machine-fingerprint "a1b2c3d4..." \
    --output license.json
```

---

## 5. Deployment & Packaged Executable Setup

When you package your project as a standalone Windows build (e.g., `WindowsNoEditor` / `Windows` Shipping):

1. **Place `license.json`** in either:
   - The root directory next to the main launcher `.exe` (e.g. `Windows/MyGame.exe`).
   - Or in the binary folder: `Windows/MyGame/Binaries/Win64/license.json`.

2. **Startup Behavior:**
   - **Valid License:** Shows a success confirmation dialog (or starts directly) and enters gameplay.
   - **Invalid / Expired / Missing / Mismatched License:** Displays a native Windows modal error dialog explaining the exact reason without revealing sensitive file paths or hardware hashes, and cleanly exits before loading gameplay.
