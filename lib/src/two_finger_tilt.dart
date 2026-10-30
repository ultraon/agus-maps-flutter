import 'dart:ui';

/// What a two-finger gesture on the map does.
enum TwoFingerGesture {
  /// The fingers have not moved far enough to tell.
  undecided,

  /// Both fingers move up or down together: the map tilts.
  tilt,

  /// Any other move: the engine zooms and rotates the map.
  pinch,
}

/// Tells a two-finger tilt from a pinch or a rotation, from where the two
/// fingers started and where they are now, in logical pixels.
///
/// A tilt needs two fingers side by side that both move up, or both move
/// down, mostly vertically, while the distance between them stays about the
/// same. The first move past [slop] decides; the caller keeps that choice for
/// the rest of the gesture.
class TwoFingerTiltDetector {
  /// A finger must move this far before the gesture is decided.
  final double slop;

  /// For a tilt, each finger moves at least this many times more vertically
  /// than horizontally.
  final double verticalRatio;

  /// For a tilt, the distance between the fingers changes by at most this
  /// part of the vertical move.
  final double maxSpreadChange;

  const TwoFingerTiltDetector({
    this.slop = 12,
    this.verticalRatio = 2,
    this.maxSpreadChange = 0.5,
  });

  TwoFingerGesture classify({
    required Offset start1,
    required Offset start2,
    required Offset now1,
    required Offset now2,
  }) {
    final move1 = now1 - start1;
    final move2 = now2 - start2;
    if (move1.distance < slop && move2.distance < slop) {
      return TwoFingerGesture.undecided;
    }
    final between = start2 - start1;
    final sideBySide = between.dy.abs() <= between.dx.abs();
    final bothVertical = _isVertical(move1) &&
        _isVertical(move2) &&
        move1.dy.sign == move2.dy.sign;
    final smallestRise =
        move1.dy.abs() < move2.dy.abs() ? move1.dy.abs() : move2.dy.abs();
    final spreadChange = ((now2 - now1).distance - between.distance).abs();
    final isTilt = sideBySide &&
        bothVertical &&
        smallestRise >= slop / 2 &&
        spreadChange <= maxSpreadChange * smallestRise;
    return isTilt ? TwoFingerGesture.tilt : TwoFingerGesture.pinch;
  }

  bool _isVertical(Offset move) =>
      move.dy != 0 && move.dy.abs() >= verticalRatio * move.dx.abs();
}

/// The tilt change for each logical pixel of a two-finger drag. A drag over
/// about a third of a phone screen goes from flat to the largest tilt.
const double kTiltDegreesPerPixel = 0.25;

/// The tilt in degrees after the fingers moved [dragDy] logical pixels down
/// from where the tilt began at [startAngle]. A drag up tilts the map more,
/// as in Google Maps. The result stays between 0 (flat) and [maxAngle].
double tiltAngleForDrag({
  required double startAngle,
  required double dragDy,
  required double maxAngle,
  double degreesPerPixel = kTiltDegreesPerPixel,
}) {
  final angle = startAngle - dragDy * degreesPerPixel;
  if (angle < 0) return 0;
  return angle > maxAngle ? maxAngle : angle;
}
