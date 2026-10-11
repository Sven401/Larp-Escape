package de.larpescape.companion.data.device

/** Counts scan callbacks without retaining names or addresses of unrelated devices. */
class BleScanEvidenceV1 {
    private val productAddresses = HashSet<String>()
    var callbacks: Int = 0
        private set
    var filtered: Int = 0
        private set

    fun callback() { if (callbacks < 100000) callbacks++ }
    fun filtered() { if (filtered < 100000) filtered++ }

    /** Only a canonical product-service advertisement may enter the pairing report. */
    fun productCandidate(address: String, advertisesCanonicalService: Boolean): Boolean {
        if (!advertisesCanonicalService || !Regex("[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}").matches(address)) return false
        if (productAddresses.size >= 16) return false
        return productAddresses.add(address.uppercase())
    }

    fun summary(reason: String): String {
        require(reason in setOf("complete", "failed", "stopped"))
        val outcome = when {
            callbacks == 0 -> "no_callbacks"
            productAddresses.isNotEmpty() -> "product_advertisement_seen"
            filtered == callbacks -> "callbacks_filtered"
            else -> "no_product_advertisement"
        }
        return "reason=$reason;result=$outcome;callbacks=$callbacks;filtered=$filtered;productCandidates=${productAddresses.size}"
    }
}
