step_3d = 450;
step_2d = 450;

Point(1) = {-10000, 10000, -10000, step_3d};
Point(2) = {-10000, -10000, -10000, step_3d};
Point(3) = {10000, -10000, -10000, step_3d};
Point(4) = {10000, 10000, -10000, step_3d};
Point(5) = {-10000, 10000, 10000, step_3d};
Point(6) = {-10000, -10000, 10000, step_3d};
Point(7) = {10000, -10000, 10000, step_3d};
Point(8) = {10000, 10000, 10000, step_3d};
Point(11) = {-10000, -10000, -10000, step_2d};
Point(12) = {10000, -10000, -10000, step_2d};
Point(13) = {10000, 10000, -10000, step_2d};
Point(14) = {-10000, 10000, -10000, step_2d};
Point(15) = {-10000, 10000, 10000, step_2d};
Point(16) = {10000, 10000, 10000, step_2d};
Point(17) = {-10000, -10000, 10000, step_2d};
Point(18) = {10000, -10000, 10000, step_2d};
Point(19) = {0, 0, -10000, step_2d};
Point(20) = {0, 0, 10000, step_2d};
Line(1) = {1, 2};
Line(2) = {6, 2};
Line(3) = {1, 5};
Line(4) = {4, 1};
Line(5) = {5, 8};
Line(6) = {4, 8};
Line(7) = {5, 6};
Line(8) = {2, 3};
Line(9) = {4, 3};
Line(10) = {3, 7};
Line(11) = {7, 8};
Line(12) = {7, 6};
Line(24) = {15, 20};
Line(25) = {20, 18};
Line(26) = {18, 12};
Line(27) = {12, 19};
Line(28) = {19, 20};
Line(29) = {19, 14};
Line(30) = {14, 15};
Line(31) = {17, 11};
Line(32) = {17, 20};
Line(33) = {11, 19};
Line(34) = {20, 16};
Line(35) = {13, 16};
Line(36) = {13, 19};
Line Loop(14) = {5, -11, 12, -7};
Plane Surface(14) = {14};
Line Loop(16) = {3, 7, 2, -1};
Plane Surface(16) = {16};
Line Loop(18) = {8, 10, 12, 2};
Plane Surface(18) = {18};
Line Loop(20) = {4, 1, 8, -9};
Plane Surface(20) = {20};
Line Loop(22) = {6, -11, -10, -9};
Plane Surface(22) = {22};
Line Loop(24) = {5, -6, 4, 3};
Plane Surface(24) = {24};
Line Loop(38) = {24, -28, 29, 30};
Plane Surface(38) = {38};
Line Loop(40) = {28, 34, -35, 36};
Plane Surface(40) = {40};
Line Loop(42) = {28, 25, 26, 27};
Plane Surface(42) = {42};
Line Loop(44) = {28, -32, 31, 33};
Plane Surface(44) = {44};
Surface Loop(26) = {14, 16, 18, 20, 22, 24};
Volume(26) = {26};
Physical Surface(".3d_sides") = {16, 18, 22, 24};
Physical Surface(".3d_top") = {14};
Physical Surface(".3d_bottom") = {20};
Physical Surface("2d") = {38, 40, 42, 44};
Physical Volume("3d") = {26};
