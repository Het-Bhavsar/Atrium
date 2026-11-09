import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/config.dart';
import 'package:atrium/core/routes/app_router.dart';
import 'package:atrium/core/theme/app_theme.dart';
import 'package:atrium/features/device/presentation/device_sheets.dart';
import 'package:atrium/features/insights/data/metrics_provider.dart';
import 'package:atrium/features/insights/domain/sleep_model.dart';
import 'package:atrium/features/pod/data/pod_api.g.dart';
import 'package:atrium/features/pod/data/pod_session.dart';
import 'package:atrium/features/pod/domain/pod.dart';
import 'package:atrium/features/profile/data/profile_store.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';
import 'package:path_provider/path_provider.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  // Readings and the profile stay on this phone: keep the data folder out of device and cloud backups.
  try {
    await FilesHostApi().keepOnDevice((await getApplicationSupportDirectory()).path);
  } on Object catch (e) {
    debugPrint('keepOnDevice: $e');
  }
  final store = await ProfileStore.open();
  final saved = await store.load();
  // The sleep model's training data asks for attribution (ODC-By 1.0); it shows in Settings > Open source.
  LicenseRegistry.addLicense(
    () => Stream.value(
      const LicenseEntryWithLineBreaks(['Sleep model training data'], 'The sleep and wake model (sleep_v1) was trained on the sleep-accel dataset 1.0.0, used under the Open Data Commons Attribution License (ODC-By) 1.0.'),
    ),
  );
  // The sleep model is data: a retrained file ships without a code change.
  final models = kRealData ? SleepModels.parse(await rootBundle.loadString(SleepModels.asset)) : null;
  runApp(
    ProviderScope(
      overrides: [profileStoreProvider.overrideWithValue(store), savedProfileProvider.overrideWithValue(saved), sleepModelsProvider.overrideWithValue(models)],
      child: const AtriumApp(),
    ),
  );
}

class AtriumApp extends ConsumerStatefulWidget {
  const AtriumApp({super.key});

  @override
  ConsumerState<AtriumApp> createState() => _AtriumAppState();
}

class _AtriumAppState extends ConsumerState<AtriumApp> {
  late final GoRouter _router = buildRouter(initial: kStartRoute, onQuery: kDebugMode ? _applyQuery : null);

  /// Debug builds: the HTML prototype's URL switches (build, dev, data, glasses, labs, sheet), so each screen
  /// and state can be opened by deep link for side-by-side checks.
  void _applyQuery(Map<String, String> q) {
    const dev = {
      'worn': PodSituation.worn,
      'offskin': PodSituation.offSkin,
      'charging': PodSituation.charging,
      'full': PodSituation.full,
      'cold': PodSituation.cold,
      'syncdue': PodSituation.syncDue,
      'notfound': PodSituation.notFound,
    };
    Future.microtask(() {
      ref
          .read(appStateProvider.notifier)
          .update(
            (s) => s.copyWith(
              // Like the prototype URL, a key left out means its default.
              build: q['build'] == 'evt' ? Build.evt : Build.v1,
              situation: dev[q['dev']] ?? PodSituation.worn,
              // Real-data builds work the data state out from the log; only an explicit key overrides it.
              data: DataState.values.asNameMap()[q['data']] ?? (kRealData ? s.data : DataState.normal),
              glasses: q['glasses'] == null ? null : q['glasses'] == 'yes',
              labs: q['labs'] == '1',
            ),
          );
      final y = double.tryParse(q['y'] ?? '');
      if (y != null) {
        // Scroll every vertical list on screen to y (debug capture of long pages).
        WidgetsBinding.instance.addPostFrameCallback((_) {
          void visit(Element e) {
            if (e is StatefulElement && e.state is ScrollableState) {
              final p = (e.state as ScrollableState).position;
              if (p.axis == Axis.vertical && p.hasContentDimensions) p.jumpTo(y.clamp(p.minScrollExtent, p.maxScrollExtent));
            }
            e.visitChildElements(visit);
          }

          _router.routerDelegate.navigatorKey.currentContext?.visitChildElements(visit);
        });
      }
      final sheet = q['sheet'];
      if (sheet == null) return;
      WidgetsBinding.instance.addPostFrameCallback((_) {
        final ctx = _router.routerDelegate.navigatorKey.currentContext;
        if (ctx == null) return;
        if (sheet == 'box') showBoxContents(ctx);
        if (sheet == 'plinth-info') showPlinthInfo(ctx);
      });
    });
  }

  @override
  void initState() {
    super.initState();
    // Real builds: reconnect to the bonded pod and start syncing as soon as the app opens.
    if (kRealData) ref.read(podSessionProvider);
  }

  @override
  Widget build(BuildContext context) {
    final mode = ref.watch(appStateProvider.select((s) => s.themeMode));
    return MaterialApp.router(
      title: 'Atrium',
      debugShowCheckedModeBanner: false,
      theme: buildTheme(Brightness.light),
      darkTheme: buildTheme(Brightness.dark),
      themeMode: mode,
      routerConfig: _router,
      // Status bar icons follow the theme; photo pages override this with a light style.
      builder: (context, child) => AnnotatedRegion<SystemUiOverlayStyle>(
        value: Theme.of(context).brightness == Brightness.dark ? SystemUiOverlayStyle.light : SystemUiOverlayStyle.dark,
        child: child!,
      ),
    );
  }
}
