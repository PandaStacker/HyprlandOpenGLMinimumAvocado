#!/usr/bin/env bash
set -euo pipefail

echo "=========================================="
echo " Validating OpenGL GLSL Shaders"
echo "=========================================="

if ! command -v glslangValidator >/dev/null 2>&1; then
    echo "Error: glslangValidator is not installed or not in PATH."
    exit 1
fi

SHADER_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PAIRS=(
    "triangle.vert triangle.frag Basic-Triangle"
    "pbr.vert pbr.frag 3D-PBR-Cook-Torrance"
    "fps_viewmodel.vert fps_viewmodel.frag FPS-Viewmodel"
    "fps_skybox.vert fps_skybox.frag FPS-Skybox"
    "fps_reticle.vert fps_reticle.frag FPS-Reticle"
    "ui_panel.vert ui_panel.frag UI-Panel-SDF"
    "ui_text.vert ui_text.frag UI-Text-SDF"
    "sprite.vert sprite.frag 2D/3D-Sprite"
    "obj_mesh.vert obj_mesh.frag OBJ-Mesh-PBR"
)

TOTAL=0
PASSED=0

for entry in "${PAIRS[@]}"; do
    TOTAL=$((TOTAL + 1))
    read -r vert frag name <<< "$entry"
    vert_path="$SHADER_DIR/$vert"
    frag_path="$SHADER_DIR/$frag"

    printf "[%2d/%2d] Checking %-22s (%s + %s)... " "$TOTAL" "${#PAIRS[@]}" "$name" "$vert" "$frag"

    if output=$(glslangValidator -l "$vert_path" "$frag_path" 2>&1); then
        echo "OK"
        PASSED=$((PASSED + 1))
    else
        echo "FAILED"
        echo "$output"
    fi
done

echo "=========================================="
echo "Result: $PASSED / $TOTAL shader pairs passed validation."
echo "=========================================="

if [ "$PASSED" -eq "$TOTAL" ]; then
    exit 0
else
    exit 1
fi
