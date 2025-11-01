# SF2 Chromium Embedded Framework

This code contains the implementation of the latest version of the Chromium Embedded Framework in the latest Source SDK 2013 build.

> [!IMPORTANT]
> You are required to provide your own compiled build of CEF. You can download a standard distribution of it here at [the CEF automated builds website](https://cef-builds.spotifycdn.com/index.html). (You must build `libcef_dll_wrapper` yourself)

## How do I apply this onto my mod?

### 0. Copying the necessary files

Clone the repository, or download a copy of this repository.

Copy the contents of the `src` folder into your mod's source code.

> [!IMPORTANT]
> If it asks you whether to replace the files, decline the request by clicking "No", or anything similar as long that it's an decline or cancel button. :/

### 1. Configure VPC scripts

For your mod's client project (client_\*.vpc), you'll need to include the `chromium.vpc` script on the top of your client project, right after the base includes:

```diff
...
$Include "$SRCDIR\game\client\client_base.vpc"
$Include "$SRCDIR\game\client\client_replay_base.vpc"
$include "$SRCDIR\game\shared\tf\tf_gcmessages_include.vpc"
$include "$SRCDIR\game\shared\tf\tf_proto_def_messages_include.vpc"
$Include "$SRCDIR\game\client\client_econ_base.vpc"
$Include "$SRCDIR\vpc_scripts\source_saxxyawards.vpc" [!$SOURCESDK]
$Include "$SRCDIR\utils\itemtest_lib\itemtest_lib_support.vpc" [$WORKSHOP_IMPORT_ENABLE]
+$Include "$SRCDIR\game\client\chromium.vpc"
...
```

On `src\vpc_scripts\groups.vgc`, add `"cef_subprocess"` into the `game`, and `everything` groups:

```diff
...
$Group "game"
{
+   "cef_subprocess"
    "client"
    "game_shader_generic_example"
    "launcher_main"
    "mathlib"
    "matsys_controls"
    "raytrace"
    "server"
    "tier1"
    "vgui_controls"
}
...
$Group "everything"
{
    "captioncompiler"
+   "cef_subprocess"
    "client"
    "fgdlib"
    "glview"
    "height2normal"
    "launcher_main"
    "mathlib"
    "matsys_controls"
    "motionmapper"
    "phonemeextractor"
    "qc_eyes"
    "raytrace"
    "server"
    "serverplugin_empty"
    "tgadiff"
    "tier1"
    "vbsp"
    "vgui_controls"
    "vice"
    "vrad_dll"
    "vrad_launcher"
    "vtf2tga"
    "vtfdiff"
    "vvis_dll"
    "vvis_launcher"

    "game_shader_generic_example"

    // You probably don't want to override all shaders by default.
    // This is just an example mainly! Pick and choose what you want from this project. :D
    //"game_shader_generic_std"
}
...
```

The order of where the `cef_subprocess` project isn't really important, as long the `cef_subprocess` is in one of those groups, you're good to go!

On `src/vpc_scripts/projects.vgc`, add the following into any part of the file:

```diff
...
+$Project "cef_subprocess"
+{
+    "cef_subprocess\cef_subprocess_hl2mp.vpc" [$HL2MP]
+    "cef_subprocess\cef_subprocess_tf.vpc" [$TF]
+}
...
```

Make sure to save all of your changes!

### 3. Build Build Build

Since we have changed our VPC scripts, run `createallprojects.bat` at least once.

Open the `everything.sln` solution with Visual Studio 2022.

Now once you have the solution open, you can now build it by using the "you already know ever since the start of this tutorial" technique.

### 4. Render the browser into your VGUI panel

If you want to render the browser into one of your VGUI panel, you can add the following into your panel's header:

```cpp
...
#include "cef/cef_browser.h"
...
class CMyPanel : public vgui::Frame
{
...
private:
    CCefBrowser *m_pBrowser;
};
...
```

Initialize it in your panel's constructor:

```cpp
CMyPanel::CMyPanel(vgui::Panel *parent)
    : BaseClass(parent, "MyPanel")
{
    m_pBrowser = new CCefBrowser("MyBrowser", "https://google.com");
    m_pBrowser->GetPanel()->SetParent(this);

    // must be called later after the browser has been created.
    m_pBrowser->SetPos(10, 10);
    m_pBrowser->SetSize(GetWide()-10, GetTall()-10);
}
```

----

And that's all you gotta do! Test it in game and see if it really works, if it doesn't that's not my problem now ;) /jk

## Credits

- [Lambda Wars](https://github.com/Sandern/lambdawars) - Used their CEF implementation as the base of this implementation

## License

This implementation is licensed under the CC BY-NC 3.0 license, same licensed used in the Lambda Wars codebase.
