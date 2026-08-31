/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "Ieum", "index.html", [
    [ "Ieum Developer Guide", "index.html#autotoc_md12", null ],
    [ "Building Ieum", "md_docs_2dev_2build.html", [
      [ "Configuration", "md_docs_2dev_2build.html#autotoc_md1", [
        [ "Windows Configuration", "md_docs_2dev_2build.html#autotoc_md2", [
          [ "Windows and Qt", "md_docs_2dev_2build.html#autotoc_md3", [
            [ "System Qt", "md_docs_2dev_2build.html#autotoc_md4", null ],
            [ "vcpkg managed Qt", "md_docs_2dev_2build.html#autotoc_md5", null ]
          ] ]
        ] ],
        [ "macOS code signing", "md_docs_2dev_2build.html#autotoc_md6", null ]
      ] ],
      [ "Build", "md_docs_2dev_2build.html#autotoc_md7", null ],
      [ "Install", "md_docs_2dev_2build.html#autotoc_md8", null ],
      [ "Making Ieum packages", "md_docs_2dev_2build.html#autotoc_md9", null ]
    ] ],
    [ "Contributing to Ieum", "contributing_guide.html", [
      [ "Read the Full Guidelines", "contributing_guide.html#autotoc_md10", null ],
      [ "Static Analysis Workflows", "contributing_guide.html#autotoc_md11", null ]
    ] ],
    [ "pro_local_file_transfer", "md_docs_2dev_2pro__local__file__transfer.html", [
      [ "Pro Local 파일 전송", "md_docs_2dev_2pro__local__file__transfer.html#autotoc_md13", [
        [ "준비물", "md_docs_2dev_2pro__local__file__transfer.html#autotoc_md14", null ],
        [ "활성화", "md_docs_2dev_2pro__local__file__transfer.html#autotoc_md15", null ],
        [ "동작과 안전 경계", "md_docs_2dev_2pro__local__file__transfer.html#autotoc_md16", null ],
        [ "오프라인 라이선스의 의미", "md_docs_2dev_2pro__local__file__transfer.html#autotoc_md17", null ],
        [ "문제 해결", "md_docs_2dev_2pro__local__file__transfer.html#autotoc_md18", null ]
      ] ]
    ] ],
    [ "Protocol Reference", "protocol_reference.html", [
      [ "Protocol Overview", "protocol_reference.html#autotoc_md19", [
        [ "Key Implementation Files", "protocol_reference.html#autotoc_md20", null ]
      ] ],
      [ "Protocol Architecture", "protocol_reference.html#autotoc_md21", null ],
      [ "Protocol State Machine", "protocol_reference.html#autotoc_md22", [
        [ "State Descriptions", "protocol_reference.html#autotoc_md23", null ]
      ] ],
      [ "Message Categories", "protocol_reference.html#autotoc_md24", null ],
      [ "Message Reference Table", "protocol_reference.html#autotoc_md25", null ],
      [ "Typical Control Flow", "protocol_reference.html#autotoc_md26", null ],
      [ "Protocol Constraints", "protocol_reference.html#autotoc_md27", [
        [ "Message and Data Size Limits", "protocol_reference.html#autotoc_md28", null ],
        [ "TLS Handshake and Security (Protocol v1.4+)", "protocol_reference.html#autotoc_md29", null ],
        [ "Key Code and Modifier Mapping", "protocol_reference.html#autotoc_md30", null ]
      ] ],
      [ "Timing and Synchronization", "protocol_reference.html#autotoc_md31", [
        [ "Keep-Alive Mechanism (Protocol v1.3+)", "protocol_reference.html#autotoc_md32", null ],
        [ "Synchronization on Screen Entry", "protocol_reference.html#autotoc_md33", null ],
        [ "Handshake Timeout", "protocol_reference.html#autotoc_md34", null ]
      ] ],
      [ "Version Compatibility", "protocol_reference.html#autotoc_md35", [
        [ "Version Migration Guide", "protocol_reference.html#autotoc_md36", null ]
      ] ],
      [ "Implementation Examples", "protocol_reference.html#autotoc_md37", [
        [ "Connection Lifecycle", "protocol_reference.html#autotoc_md38", null ],
        [ "Message Handling", "protocol_reference.html#autotoc_md39", null ],
        [ "Complete Message Exchange Sequence", "protocol_reference.html#autotoc_md40", null ]
      ] ],
      [ "Debugging and Troubleshooting", "protocol_reference.html#autotoc_md41", [
        [ "Common Issues", "protocol_reference.html#autotoc_md42", null ],
        [ "Debug Tools", "protocol_reference.html#autotoc_md43", null ]
      ] ],
      [ "Platform-Specific Implementations", "protocol_reference.html#autotoc_md44", null ],
      [ "Implementation Checklist", "protocol_reference.html#autotoc_md45", [
        [ "Basic Client Implementation", "protocol_reference.html#autotoc_md46", null ],
        [ "Advanced Features", "protocol_reference.html#autotoc_md47", null ]
      ] ],
      [ "Reference Implementation", "protocol_reference.html#autotoc_md48", null ],
      [ "Contributing", "protocol_reference.html#autotoc_md49", null ],
      [ "Support and Resources", "protocol_reference.html#autotoc_md50", null ],
      [ "Ieum Input Language Extension (v1.9)", "protocol_reference.html#autotoc_md52", [
        [ "Canonical scancode flag (v1.10)", "protocol_reference.html#autotoc_md53", null ]
      ] ]
    ] ],
    [ "Public privacy guard", "md_docs_2dev_2public-privacy-guard.html", [
      [ "Private inputs", "md_docs_2dev_2public-privacy-guard.html#autotoc_md55", null ],
      [ "Install and verify the pre-push hook", "md_docs_2dev_2public-privacy-guard.html#autotoc_md56", null ],
      [ "CI and release boundaries", "md_docs_2dev_2public-privacy-guard.html#autotoc_md57", null ]
    ] ],
    [ "update-delivery", "md_docs_2dev_2update-delivery.html", [
      [ "Safe update delivery", "md_docs_2dev_2update-delivery.html#autotoc_md58", [
        [ "Release prerequisites", "md_docs_2dev_2update-delivery.html#autotoc_md59", null ],
        [ "Update sequence", "md_docs_2dev_2update-delivery.html#autotoc_md60", null ],
        [ "Acceptance criteria", "md_docs_2dev_2update-delivery.html#autotoc_md61", null ],
        [ "Current alpha behavior", "md_docs_2dev_2update-delivery.html#autotoc_md62", null ]
      ] ]
    ] ],
    [ "Deprecated List", "deprecated.html", null ],
    [ "Topics", "topics.html", "topics" ],
    [ "Namespaces", "namespaces.html", [
      [ "Namespace List", "namespaces.html", "namespaces_dup" ],
      [ "Namespace Members", "namespacemembers.html", [
        [ "All", "namespacemembers.html", null ],
        [ "Functions", "namespacemembers_func.html", null ],
        [ "Variables", "namespacemembers_vars.html", null ],
        [ "Typedefs", "namespacemembers_type.html", null ],
        [ "Enumerations", "namespacemembers_enum.html", null ]
      ] ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", "functions_vars" ],
        [ "Typedefs", "functions_type.html", null ],
        [ "Enumerations", "functions_enum.html", null ],
        [ "Enumerator", "functions_eval.html", null ],
        [ "Related Symbols", "functions_rela.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", "globals_dup" ],
        [ "Functions", "globals_func.html", null ],
        [ "Variables", "globals_vars.html", "globals_vars" ],
        [ "Typedefs", "globals_type.html", null ],
        [ "Enumerations", "globals_enum.html", null ],
        [ "Enumerator", "globals_eval.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"AboutDialog_8cpp.html",
"CoreIpc_8cpp.html#a08612c9bfeea951841734968b93f6d5d",
"IpcServer_8h_source.html",
"Log_8h.html#a05785244b5afab2f72b74f922dfb777b",
"MouseTypes_8h.html#a38bb5aaf660cd0a9217897febf8d4eee",
"PortalRemoteDesktop_8cpp.html",
"XWindowsClipboardBMPConverter_8h_source.html",
"classArchMultithreadPosix.html#ab499afbb6499d43c901b7290bb4c2042",
"classClient.html#a9d96f94fb74422ebc06052257c67a2bc",
"classClient_1_1FailInfo.html#a72646ec8eb08f7a4b000f9f898325ad7",
"classIArchNetwork.html#a258d8bbfbf1d47ab8bb5e83d2e0f3230",
"classIScreen.html#a3bc2359e5bea9d21affa5f043269a677",
"classKeyState.html#ac90aea1f3eeb0dff6bd208f8983bffdf",
"classMSWindowsScreen.html#aea93c7d6f0abd90b4231970c516fe6cf",
"classPlatformScreen.html#a0951699aff03fab5d2d2c3baa50f11e6",
"classScreenSetupView.html#aaf0b0780e68a78c2df24551f02c2bd5f",
"classSettings.html#ace87ffc019dee661124504049eae06c9",
"classTrashScreenWidget.html#a2e5944864a2fba0b610e854d5eac0ace",
"classXWindowsScreenSaver.html#a0eda0eed06e3b4abf3f325c5df70ea36",
"classdeskflow_1_1KeyMap.html#a66b39a18d7c46803ee4dd50c0ccd3dc1",
"classdeskflow_1_1Screen.html#afdc0fc09e46aa8b4af3d8d1a611e61cc",
"classdeskflow_1_1gui_1_1CoreProcess.html#ae9771a6cd17446caa6a47c28e58abf80",
"classdeskflow_1_1server_1_1Config.html#a31c76775c691d6edf36d8d4f00427f5f",
"contributing_guide.html",
"group__protocol__mouse.html#gad981fdfa385cbe52050fa58d17c1a273",
"namespacedeskflow_1_1filetransfer.html#a55809f4972c09ac74926df05f2766f18a9553ea123143a37410225637ac96a171",
"namespacedeskflow_1_1server_1_1cursor.html#a944ba085865908c377ffa52e408f6876",
"structSettings_1_1Log.html#a8a5b078f91feb4dc4029194a32094481",
"structdeskflow_1_1filetransfer_1_1FileTransferDataRoute.html#a8f3204162ef5f477c5b070672c1f4d80",
"structdeskflow_1_1filetransfer_1_1MSWindowsFileTransferReader_1_1Impl.html#aa655c48241a4084a87278d9d28b17251",
"structdeskflow_1_1network_1_1TailscaleIntegration_1_1ProcessCallbacks.html"
];

const SYNCONMSG = 'click to disable panel synchronization';
const SYNCOFFMSG = 'click to enable panel synchronization';
const LISTOFALLMEMBERS = 'List of all members';