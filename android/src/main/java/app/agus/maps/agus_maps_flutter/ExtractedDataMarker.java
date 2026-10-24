package app.agus.maps.agus_maps_flutter;

import android.content.res.AssetManager;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Tracks which engine data version the plugin extracted into the files directory.
 *
 * <p>The marker file holds the data version (the "v" of the bundled countries.txt) of the last
 * full extraction. An app update that bundles newer engine data changes this version, so the
 * plugin extracts the data files and the bundled maps again. Without this, the new engine reads
 * the data files of the old engine and can crash.
 */
final class ExtractedDataMarker {
  static final String MARKER_NAME = ".comaps_data_extracted";

  private static final String COUNTRIES_ASSET = "assets/comaps_data/countries.txt";
  private static final Pattern VERSION_PATTERN = Pattern.compile("\"v\"\\s*:\\s*(\\d+)");
  // The version is near the top of countries.txt, so the file need not be read in full.
  private static final int VERSION_READ_LIMIT = 4096;
  private static final int COPY_BUFFER_SIZE = 32 * 1024;

  private final AssetManager assetManager;
  private final File filesDir;
  private boolean bundledVersionRead = false;
  @Nullable private String bundledVersion;

  ExtractedDataMarker(@NonNull AssetManager assetManager, @NonNull File filesDir) {
    this.assetManager = assetManager;
    this.filesDir = filesDir;
  }

  /** The data version of the bundled engine data, or null when it cannot be read. */
  @Nullable
  synchronized String getBundledVersion() {
    if (!bundledVersionRead) {
      bundledVersion = readBundledVersion();
      bundledVersionRead = true;
    }
    return bundledVersion;
  }

  /** The data version stored in the marker, or null when the marker is missing or empty. */
  @Nullable
  String getExtractedVersion() {
    File marker = new File(filesDir, MARKER_NAME);
    if (!marker.exists()) {
      return null;
    }
    try {
      String content = new String(readAll(marker), StandardCharsets.UTF_8).trim();
      return content.isEmpty() ? null : content;
    } catch (IOException e) {
      android.util.Log.w("AgusMapsFlutter", "Cannot read " + marker, e);
      return null;
    }
  }

  /**
   * Says whether the extracted files belong to the bundled engine data. When the bundled version
   * cannot be read, any marker counts as current, which is the behaviour before versioned markers.
   */
  boolean isExtractedDataCurrent() {
    String bundled = getBundledVersion();
    if (bundled == null) {
      return new File(filesDir, MARKER_NAME).exists();
    }
    return bundled.equals(getExtractedVersion());
  }

  void deleteMarker() {
    File marker = new File(filesDir, MARKER_NAME);
    if (marker.exists() && !marker.delete()) {
      android.util.Log.w("AgusMapsFlutter", "Cannot delete " + marker);
    }
  }

  /** Writes the bundled version into the marker. Call it only after a full extraction. */
  void writeMarker() throws IOException {
    String bundled = getBundledVersion();
    byte[] content = (bundled == null ? "" : bundled).getBytes(StandardCharsets.UTF_8);
    File marker = new File(filesDir, MARKER_NAME);
    File tmp = new File(filesDir, MARKER_NAME + ".tmp");
    try (OutputStream out = new FileOutputStream(tmp)) {
      out.write(content);
    }
    replace(tmp, marker);
  }

  /** Copies an asset to a temporary file first, so a failed copy never leaves a broken target. */
  void copyAssetReplacing(@NonNull String assetLookupKey, @NonNull File target) throws IOException {
    File tmp = new File(target.getParentFile(), target.getName() + ".tmp");
    try (InputStream in = assetManager.open(assetLookupKey);
        OutputStream out = new FileOutputStream(tmp)) {
      byte[] buffer = new byte[COPY_BUFFER_SIZE];
      int read;
      while ((read = in.read(buffer)) != -1) {
        out.write(buffer, 0, read);
      }
    } catch (IOException e) {
      tmp.delete();
      throw e;
    }
    replace(tmp, target);
  }

  private static void replace(File source, File target) throws IOException {
    if (!source.renameTo(target)) {
      source.delete();
      throw new IOException("Cannot replace " + target);
    }
  }

  @Nullable
  private String readBundledVersion() {
    String key = io.flutter.FlutterInjector.instance().flutterLoader()
        .getLookupKeyForAsset(COUNTRIES_ASSET);
    try (InputStream in = assetManager.open(key)) {
      byte[] buffer = new byte[VERSION_READ_LIMIT];
      int total = 0;
      int read;
      while (total < buffer.length && (read = in.read(buffer, total, buffer.length - total)) != -1) {
        total += read;
      }
      Matcher matcher = VERSION_PATTERN.matcher(new String(buffer, 0, total, StandardCharsets.UTF_8));
      return matcher.find() ? matcher.group(1) : null;
    } catch (IOException e) {
      android.util.Log.w("AgusMapsFlutter", "Cannot read the bundled data version", e);
      return null;
    }
  }

  private static byte[] readAll(File file) throws IOException {
    try (InputStream in = new FileInputStream(file)) {
      ByteArrayOutputStream out = new ByteArrayOutputStream();
      byte[] buffer = new byte[256];
      int read;
      while ((read = in.read(buffer)) != -1) {
        out.write(buffer, 0, read);
      }
      return out.toByteArray();
    }
  }
}
