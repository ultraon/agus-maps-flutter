import Foundation

/// Tracks which engine data version the plugin extracted into the Documents directory.
///
/// The marker file holds the data version (the "v" of the bundled countries.txt) of the last
/// full extraction. An app update that bundles newer engine data changes this version, so the
/// plugin extracts the data files and the bundled maps again. Without this, the new engine reads
/// the data files of the old engine and can crash.
struct ExtractedDataMarker {
    static let markerName = ".comaps_data_extracted"

    // The version is near the top of countries.txt, so the file need not be read in full.
    private static let versionReadLimit = 4096

    let bundledVersion: String?
    private let markerURL: URL

    init(documentsDir: URL, bundledCountriesPath: String?) {
        markerURL = documentsDir.appendingPathComponent(ExtractedDataMarker.markerName)
        bundledVersion = ExtractedDataMarker.readVersion(countriesPath: bundledCountriesPath)
    }

    /// The data version stored in the marker, or nil when the marker is missing or empty.
    func extractedVersion() -> String? {
        guard let data = try? Data(contentsOf: markerURL),
              let text = String(data: data, encoding: .utf8) else {
            return nil
        }
        let version = text.trimmingCharacters(in: .whitespacesAndNewlines)
        return version.isEmpty ? nil : version
    }

    /// Says whether the extracted files belong to the bundled engine data. When the bundled
    /// version cannot be read, any marker counts as current, which is the behaviour before
    /// versioned markers.
    func isExtractedDataCurrent() -> Bool {
        guard let bundled = bundledVersion else {
            return FileManager.default.fileExists(atPath: markerURL.path)
        }
        return bundled == extractedVersion()
    }

    /// Writes the bundled version into the marker. Call it only after a full extraction.
    func writeMarker() throws {
        try Data((bundledVersion ?? "").utf8).write(to: markerURL, options: .atomic)
    }

    /// Copies a file to a temporary file first, so a failed copy never leaves a broken target.
    static func copyReplacing(source: String, target: URL) throws {
        let fileManager = FileManager.default
        let tmp = target.appendingPathExtension("tmp")
        try? fileManager.removeItem(at: tmp)
        try fileManager.copyItem(atPath: source, toPath: tmp.path)
        if fileManager.fileExists(atPath: target.path) {
            _ = try fileManager.replaceItemAt(target, withItemAt: tmp)
        } else {
            try fileManager.moveItem(at: tmp, to: target)
        }
    }

    private static func readVersion(countriesPath: String?) -> String? {
        guard let path = countriesPath, let handle = FileHandle(forReadingAtPath: path) else {
            return nil
        }
        defer { handle.closeFile() }
        let head = handle.readData(ofLength: versionReadLimit)
        guard let text = String(data: head, encoding: .utf8),
              let regex = try? NSRegularExpression(pattern: "\"v\"\\s*:\\s*(\\d+)"),
              let match = regex.firstMatch(in: text, range: NSRange(text.startIndex..., in: text)),
              let range = Range(match.range(at: 1), in: text) else {
            return nil
        }
        return String(text[range])
    }
}
