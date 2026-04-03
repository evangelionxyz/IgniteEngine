# Copilot Instructions

## Project Guidelines
- When calling AssetImporter::ImportTexture directly, pass a full filepath by prefixing with the project's asset directory (Project::GetAssetFilepath for relative metadata paths).
- When editing assets for example move, copy, import, we need to update to asset manager and serialize to .ixreg file in the project directory

