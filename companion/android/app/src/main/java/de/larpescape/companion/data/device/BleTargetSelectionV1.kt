package de.larpescape.companion.data.device

/** Advertisement fields are discovery hints; this result never confirms identity or access. */
fun classifyBleTargetV1(
    mode: BleScanModeV1,
    name: String,
    advertisesCanonical: Boolean,
    advertisesLab: Boolean,
    labAllowed: Boolean,
): DeviceTargetKindV1? {
    if (advertisesCanonical) return DeviceTargetKindV1.PRODUCT_ADVERTISED
    if (mode == BleScanModeV1.UNVERIFIED_DIAGNOSTIC) return DeviceTargetKindV1.UNVERIFIED
    if (labAllowed && advertisesLab) return DeviceTargetKindV1.LAB_ADVERTISED
    if (advertisesLab || !name.contains("Larp", true) && !name.contains("ESP", true)) return null
    return DeviceTargetKindV1.UNVERIFIED
}

/** Diagnostic-only candidates must be checked against the canonical GATT service. */
fun acceptsGattServiceV1(kind: DeviceTargetKindV1, canonicalPresent: Boolean, labPresent: Boolean): Boolean =
    canonicalPresent || kind == DeviceTargetKindV1.LAB_ADVERTISED && labPresent
