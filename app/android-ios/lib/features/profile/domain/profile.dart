// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:flutter/foundation.dart';

/// The person wearing Atrium. Kept only on this phone (Atrium has no accounts and no cloud).
/// Age and size set the heart rate zones and Effort.
@immutable
class Profile {
  const Profile({this.name = '', this.age, this.heightCm, this.weightKg});

  /// The prototype's sample person, used by demo builds until a profile is saved.
  static const Profile demo = Profile(name: 'Asha', age: 34, heightCm: 162, weightKg: 58);

  final String name;
  final int? age;
  final int? heightCm;
  final int? weightKg;

  bool get hasName => name.trim().isNotEmpty;

  /// "34 years · 162 cm · 58 kg", leaving out what is not set.
  String get summary => [if (age != null) '$age years', if (heightCm != null) '$heightCm cm', if (weightKg != null) '$weightKg kg'].join(' · ');

  Map<String, Object?> toJson() => {'name': name.trim(), 'age': age, 'heightCm': heightCm, 'weightKg': weightKg};

  static Profile fromJson(Map<String, Object?> j) => Profile(
    name: (j['name'] as String?) ?? '',
    age: (j['age'] as num?)?.toInt(),
    heightCm: (j['heightCm'] as num?)?.toInt(),
    weightKg: (j['weightKg'] as num?)?.toInt(),
  );

  /// Plausible ranges for the form; null means "leave empty".
  static String? checkAge(int? v) => v == null || (v >= 13 && v <= 100) ? null : 'Between 13 and 100';
  static String? checkHeight(int? v) => v == null || (v >= 100 && v <= 230) ? null : 'Between 100 and 230 cm';
  static String? checkWeight(int? v) => v == null || (v >= 30 && v <= 250) ? null : 'Between 30 and 250 kg';

  @override
  bool operator ==(Object other) => other is Profile && other.name == name && other.age == age && other.heightCm == heightCm && other.weightKg == weightKg;

  @override
  int get hashCode => Object.hash(name, age, heightCm, weightKg);
}
