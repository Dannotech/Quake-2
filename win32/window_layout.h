/* Physical window placement; the renderer retains its logical dimensions. */
#ifndef Q2_WINDOW_LAYOUT_H
#define Q2_WINDOW_LAYOUT_H

static RECT VID_FitWindowRect(RECT bounds, RECT frame, int width, int height)
{
	RECT result;
	int borderWidth = frame.right - frame.left;
	int borderHeight = frame.bottom - frame.top;
	int availableWidth = bounds.right - bounds.left - borderWidth;
	int availableHeight = bounds.bottom - bounds.top - borderHeight;
	double scale = 1.0;
	int outerWidth, outerHeight;

	if (availableWidth < 1) availableWidth = 1;
	if (availableHeight < 1) availableHeight = 1;
	if (width > availableWidth) scale = (double)availableWidth / width;
	if (height * scale > availableHeight) scale = (double)availableHeight / height;
	width = (int)(width * scale);
	height = (int)(height * scale);
	if (width < 1) width = 1;
	if (height < 1) height = 1;
	outerWidth = width + borderWidth;
	outerHeight = height + borderHeight;
	result.left = bounds.left + (bounds.right - bounds.left - outerWidth) / 2;
	result.top = bounds.top + (bounds.bottom - bounds.top - outerHeight) / 2;
	result.right = result.left + outerWidth;
	result.bottom = result.top + outerHeight;
	return result;
}

#endif
