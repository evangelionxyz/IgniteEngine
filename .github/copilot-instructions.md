# Copilot Instructions

## Project Guidelines
- When calling AssetImporter::ImportTexture directly, pass a full filepath by prefixing with the project's asset directory (Project::GetAssetFilepath for relative metadata paths).
- When editing assets for example move, copy, import, we need to update to asset manager and serialize to .ixreg file in the project directory.
- Do not call PopLayer from EditorLayer::OnDetach because Application handles layer-stack teardown and double deletion can occur.
- Add logger/assertion instrumentation when investigating runtime issues like leaks to enhance debugging and ensure stability.

## Rendering Optimization
- For rendering optimization tasks, keep SceneRenderer focused on orchestration and move processing/caching logic (like texture/material handle caching) into Renderer2D; avoid editor/gameplay camera reuse optimizations. Focus only on texture caching in Quad2DView.

