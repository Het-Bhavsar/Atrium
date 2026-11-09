import 'package:atrium/core/config.dart';
import 'package:atrium/core/util/format.dart';
import 'package:atrium/features/insights/domain/metrics_source.dart';

/// The serif sentence at the top of each page. Demo builds keep the prototype's written stories; real builds
/// have no story writer yet, so they state only what the synced log shows (wellness wording, no claims).
String story(MetricsSource d, {required String demo, required String Function() real}) {
  if (!kRealData) return demo;
  if (d.isEmpty) return 'No readings yet. Wear Atrium tonight and this fills in by morning.';
  return real();
}

String sleepStory(MetricsSource d) =>
    d.asleep > 0 ? 'You slept ${hm(d.asleep)}, from ${clockLabel(d.bedMinute)} to ${clockLabel(d.wakeMinute)}.' : 'No sleep logged last night yet.';
