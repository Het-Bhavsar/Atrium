import 'dart:io';

import 'package:atrium/core/config.dart';
import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/core/widgets/graphics.dart';
import 'package:atrium/features/onboarding/presentation/onboarding_widgets.dart';
import 'package:atrium/features/pod/data/pod_permissions.dart';
import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

/// Onboarding 3 (#15): why Atrium asks for Bluetooth, before the system prompt.
class BluetoothView extends StatelessWidget {
  const BluetoothView({super.key});

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return OnboardingScaffold(
      content: [
        const StepsDots(2),
        ObArt(
          child: Container(
            width: 96,
            height: 96,
            alignment: Alignment.center,
            decoration: BoxDecoration(color: t.accent.withValues(alpha: 0.12), borderRadius: BorderRadius.circular(26)),
            child: AIcon('bt', size: 52, color: t.accent, stroke: 1.5),
          ),
        ),
        const ObTitle('Atrium talks to the pod over Bluetooth'),
        const ObLede('Your phone asks next. Bluetooth is used only to pair with your pod and copy its readings to this phone.'),
      ],
      footer: [
        PrimaryButton(
          'Continue',
          onPressed: () async {
            // Real builds ask now (Android: Nearby devices, or location before Android 12); on a denial the
            // "Bluetooth is off for Atrium" sheet points to Settings instead of moving on.
            if (kRealData) {
              final access = await PodPermissions.requestBluetooth();
              if (access != BluetoothAccess.granted) {
                if (context.mounted) await showBluetoothProblem(context, access);
                return;
              }
            }
            if (context.mounted) await context.push('/wake');
          },
        ),
      ],
    );
  }
}

/// Prototype sheet "bt-denied": Bluetooth permission was refused; or, on Android 8-11, location services are off.
Future<void> showBluetoothProblem(BuildContext context, BluetoothAccess access) => access == BluetoothAccess.locationOff
    ? showObSheet(
        context,
        title: 'Turn on Location',
        paragraphs: const [
          'On this version of Android, phones only find Bluetooth devices nearby while Location is on.',
          'Atrium does not read or keep your location.',
        ],
        button: 'Open Location settings',
        onButton: PodPermissions.openLocationSettings,
      )
    : showObSheet(
        context,
        title: 'Bluetooth is off for Atrium',
        paragraphs: [
          'Atrium needs Bluetooth to find your pod and copy its readings. It is used for nothing else.',
          if (Platform.isIOS)
            'Open Settings and turn on Bluetooth for Atrium.'
          else
            'Open Settings, then Permissions, and allow Nearby devices. On older phones, allow Location instead.',
        ],
        button: 'Open Settings',
        onButton: PodPermissions.openSettings,
      );
