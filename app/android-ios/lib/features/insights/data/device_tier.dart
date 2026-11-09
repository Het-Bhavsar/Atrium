// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:io';

import 'package:atrium/features/insights/domain/model_plan.dart';
import 'package:device_info_plus/device_info_plus.dart';

/// This phone's tier for the model plan, from its RAM (and Android's low-RAM flag). Mid when it can't be read.
Future<DeviceTier> probeDeviceTier() async {
  try {
    final info = DeviceInfoPlugin();
    if (Platform.isAndroid) {
      final a = await info.androidInfo;
      return tierFor(ramMb: a.physicalRamSize, lowRamDevice: a.isLowRamDevice);
    }
    if (Platform.isIOS) return tierFor(ramMb: (await info.iosInfo).physicalRamSize);
  } on Object {
    // Unknown phone: the middle plan.
  }
  return DeviceTier.mid;
}
