#pragma once

class QApplication;

namespace ev {

// Must run before QApplication is constructed so Qt can load the working IME
// bridge for the current desktop session.
void configureClientInputMethod();

// Makes every QComboBox use a list-view popup with a vertical scrollbar
// instead of the native menu popup with top/bottom scrolling arrows.
void installClientStyle(QApplication &application);

} // namespace ev
