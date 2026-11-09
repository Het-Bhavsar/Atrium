// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/core/config.dart';
import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/core/widgets/scaffolds.dart';
import 'package:atrium/features/onboarding/presentation/onboarding_widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_svg/flutter_svg.dart';
import 'package:url_launcher/url_launcher.dart';

/// Settings > Why Atrium: the story behind the name and the mark (README "Why Atrium").
class NameStoryView extends StatelessWidget {
  const NameStoryView({super.key});

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    final dark = Theme.of(context).brightness == Brightness.dark;
    Widget para(String s) => Padding(
      padding: const EdgeInsets.fromLTRB(4, 0, 4, Space.s),
      child: Text(s, style: AtriumType.body(t.ink2).copyWith(fontSize: 17, height: 1.5)),
    );
    Widget part(String label, String text) => Padding(
      padding: const EdgeInsets.symmetric(vertical: 10),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SizedBox(width: 108, child: Text(label, style: AtriumType.bodyStrong(t.ink).copyWith(fontSize: 15))),
          Expanded(child: Text(text, style: AtriumType.secondary(t.ink2).copyWith(fontSize: 15))),
        ],
      ),
    );

    return CoverPage(
      image: 'sky-day',
      title: 'Why Atrium',
      subtitle: 'The name and the mark',
      backLabel: 'Settings',
      alignment: const Alignment(0, -0.1),
      reading: const AtriumMark(width: 88),
      story: 'Open to the sky. Open to everyone.',
      children: [
        const SectionHeader('The hall'),
        para(
          'A Roman house was built around one room: the atrium. Its roof stopped short of the middle and left an '
          'opening to the sky. Daylight fell through it, rain was caught beneath it, and the household gathered '
          'under it. It was the most open room in the house, and nothing in it was hidden.',
        ),
        const SectionHeader('The heart'),
        para(
          'Your heart has two atria, the chambers that fill with blood just before each beat. Every pulse you have '
          'ever felt began there. That pulse is what Atrium reads, with light, through the skin of your temporal region.',
        ),
        const SectionHeader('Why both'),
        para(
          'The name holds both ideas. Atrium follows your heart with light, and like the hall it keeps nothing '
          'closed: every schematic, every line of firmware and every data format is public, for anyone to read, '
          'build on and improve. And like a room in your own home, what it learns stays with you. There is no '
          'account and no cloud; your readings live on your phone.',
        ),
        const SectionHeader('The mark'),
        AtriumCard(
          padding: const EdgeInsets.fromLTRB(Space.xl, Space.xxl, Space.xl, Space.m),
          child: Column(
            children: [
              SvgPicture.string(
                atriumMarkSvg(ink: dark ? '#F6F2EA' : '#111113', beam: dark ? '#FF6F5C' : '#B4372A'),
                width: 132,
                height: 132 * 280 / 320,
              ),
              const SizedBox(height: Space.l),
              part('Two arches', 'The hall, drawn as the two lobes of a heart.'),
              Divider(height: 1, color: t.hair),
              part('The gap', 'The open roof, where a soft red beam of light falls in.'),
              Divider(height: 1, color: t.hair),
              part('The floor', 'A pulse wave: the shape Atrium actually measures with light, not a hospital ECG spike.'),
            ],
          ),
        ),
        const SizedBox(height: Space.xxxl),
        const StoryText('Atrium. Open to the sky.\nOpen to everyone.', center: true),
        const SizedBox(height: Space.xxl),
        Center(
          child: Pressable(
            onTap: () => launchUrl(Uri.https(kMakerSite), mode: LaunchMode.externalApplication),
            child: Padding(
              padding: const EdgeInsets.all(Space.s),
              child: Column(
                children: [
                  const LocalForgeLogo(size: 64),
                  const SizedBox(height: Space.m),
                  Text('Designed in Ahmedabad by $kMaker', textAlign: TextAlign.center, style: AtriumType.caption(t.ink2).copyWith(fontSize: 13)),
                  const SizedBox(height: 2),
                  Text(kMakerSite, style: AtriumType.caption(t.ink3).copyWith(fontSize: 13, decoration: TextDecoration.underline, decorationColor: t.hair)),
                ],
              ),
            ),
          ),
        ),
      ],
    );
  }
}
