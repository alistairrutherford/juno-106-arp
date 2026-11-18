# Changelog

## 1.1.0 (2026-11-18)

The first version published as a GitHub release, as a universal macOS zip (signed ad hoc, not
notarized).

### Added

- **Info View:** a strip along the bottom of the editor, like Live's own Info View. Point at any
  fader, switch, section, the preset bar or the keyboard and it shows what it does and what its
  settings mean; elsewhere, a short hint.
- The editor is 64 px taller to make room (1430 × 516).
- The README's screenshot shows the editor with the Info View.

### Fixed

- Restoring a saved state now sets every parameter exactly as saved. Before, an on/off switch
  (Arp On, Arp Hold, Saw, Pulse, Chorus I and II) could keep a stray value a host had set, and
  pluginval's state test failed.

## 1.0.0

The first version: the Juno-106 style voice, chorus, the 128 factory patches, Juno-106 SysEx import and the arpeggiator. Not published as a release.
