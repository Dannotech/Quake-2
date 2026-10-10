#include <windows.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "../../win32/window_layout.h"

int main(void)
{
	RECT bounds[] = {{0,0,3840,2080}, {-3840,-200,0,1880}, {1920,100,3840,1140}};
	RECT frames[] = {{-8,-31,8,8}, {-12,-47,12,12}, {0,0,0,0}};
	int sizes[][2] = {{800,600},{1280,720},{3840,2160},{7680,4320},{600,800}};
	int i, j, k, count = 0;
	for (i = 0; i < 3; ++i) for (j = 0; j < 3; ++j) for (k = 0; k < 5; ++k) {
		RECT r = VID_FitWindowRect(bounds[i], frames[j], sizes[k][0], sizes[k][1]);
		int width = r.right-r.left-(frames[j].right-frames[j].left);
		int height = r.bottom-r.top-(frames[j].bottom-frames[j].top);
		assert(r.left >= bounds[i].left && r.top >= bounds[i].top);
		assert(r.right <= bounds[i].right && r.bottom <= bounds[i].bottom);
		assert(abs((r.left-bounds[i].left)-(bounds[i].right-r.right)) <= 1);
		assert(abs((r.top-bounds[i].top)-(bounds[i].bottom-r.bottom)) <= 1);
		assert(width <= sizes[k][0] && height <= sizes[k][1]);
		/* At most one pixel of rounding in either client dimension. */
		assert(abs(width*sizes[k][1]-height*sizes[k][0]) <= sizes[k][0]+sizes[k][1]);
		++count;
	}
	printf("PASS %d placement cases: fit, center, aspect, negative origins and DPI borders\n", count);
	return 0;
}
