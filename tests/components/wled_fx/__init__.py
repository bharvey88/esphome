import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Every effect translation unit is guarded by WLED_FX_DEFAULT_ENABLE, which the
    # component's to_code emits as a build flag. Without it the effect files compile
    # to nothing and EFFECT_GROUP_1D_A does not link. The group table itself stays in
    # test_effect_metadata.cpp so the tests control which groups are linked.
    async def to_code_testing(config):
        cg.add_build_flag("-DWLED_FX_DEFAULT_ENABLE=1")

    manifest.to_code = to_code_testing
