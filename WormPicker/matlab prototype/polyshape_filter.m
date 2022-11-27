function [isworm, a, p, l, w, lw, x, y] = polyshape_filter(contour)
% Determine if a contour is a worm based on gemotric analysis
% Argument: (nx2) matrix containing all the points of the contour
% Returns: a boolean that is true when the contour is a worm

amin = 100;
amax = 1700;
lmin = 40;
lmax = 600;
lwmin = 9;
lwmax = 50;

X = contour(:,1);
Y = contour(:,2);
X(end, :) = [];
Y(end, :) = [];
shape = polyshape(X, Y);
a = polyarea(X, Y);
p = perimeter(shape);
l = p / 2;
w = a/l;
lw = l / w;
[x, y] = centroid(shape);
isworm = (amin < a) && (a < amax) && (lmin < l) && (l < lmax) && (lwmin < lw) && (lw < lwmax);

end

