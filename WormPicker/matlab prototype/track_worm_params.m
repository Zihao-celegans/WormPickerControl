path = 'C:\WormPicker\recordings\for_debugging\mov.avi';

thresh = 89;
minsize = 70;

amin = 100;
amax = 1700;
lmin = 40;
lmax = 600;
lwmin = 9;
lwmax = 50;


mov = VideoReader(path);

outlinestack = zeros(480, 640, 0);
flickering = nan(0,0);
framenum = 0;
maxframenum = 100;
while hasFrame(mov)
    framenum = framenum + 1;
    img = readFrame(mov);
    frame = rgb2gray(img);
    [h, w, x] = size(frame);

    background = imgaussfilt(frame, 21);
    background = background - 100;
    frame = frame - background;

    frame = imgaussfilt(frame, 2);

    frame = im2bw(frame, thresh/255);
    frame = not(frame);
    frame = bwareaopen(frame, minsize);
    
    contours = bwboundaries(frame);
    warning('off','MATLAB:polyshape:repairedBySimplify');
    figure(3)
    imshow(img);
    hold on
    for i=1:size(contours)
        [N_points, ~] = size(contours{i});
        if ( N_points > 3)
            [iskeep, a, p, l, w, lw, x, y] = polyshape_filter(contours{i});
            if ( 350 < x && x < 450 && 200 < y && y < 250) %this is the flickering contour
                plot(contours{i}(:,2), contours{i}(:,1))
                flickering = [flickering; iskeep, a, p, l, w, lw, avg_angle(contours{i})];               
            end    
        end
    end
    
    hold off
    
    if framenum > maxframenum
        break
    end
    disp(framenum)
end

%%
ha = tight_subplot(7, 1, 0.05, 0.05, 0.05);

figure(1)

axes(ha(1));
plot(flickering(:, 1));
set(gca,'xtick',[]);
ylabel('isworm')

axes(ha(2));
plot(flickering(:, 2));
line([0, maxframenum],[amin,amin], 'Color', 'red')
line([0, maxframenum],[amax,amax], 'Color', 'red')
set(gca,'xtick',[]);
ylabel('Area')

axes(ha(3));
plot(flickering(:, 3));
set(gca,'xtick',[]);
ylabel('Perimeter')

axes(ha(4));
plot(flickering(:, 4));
line([0, maxframenum],[lmin,lmin], 'Color', 'red')
line([0, maxframenum],[lmax,lmax], 'Color', 'red')
set(gca,'xtick',[]);
ylim([75,125])
ylabel('Length')

axes(ha(5));
plot(flickering(:, 5));
set(gca,'xtick',[]);
ylabel('Width')

axes(ha(6));
plot(flickering(:, 6));
line([0, maxframenum],[lwmin,lwmin], 'Color', 'red')
line([0, maxframenum],[lwmax,lwmax], 'Color', 'red')
set(gca,'xtick',[]);
ylabel('Length/Width')

axes(ha(7));
plot(flickering(:, 7));
ylabel('Angle')