function avg_angle = avg_angle(contour)
%UNTITLED2 Summary of this function goes here
%   Detailed explanation goes here

avg_angle = 0;
[N_points, ~] = size(contour);
for i = 1: N_points - 3
    p1 = contour(i,:);
    p2 = contour(i+1,:);
    p3 = contour(i+2,:);
    
    v1 = p2 - p1;
    v2 = p3 - p2;
    
    costheta = dot(v1, v2) / (norm(v1) * norm(v2));
    avg_angle = avg_angle + costheta;
end
avg_angle = avg_angle / N_points;
end

