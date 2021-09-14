clc;
clear all;
close all;

f1 = csvread('scope_0_1_0.csv',3,0);
f1(:,1) = f1(:,1) + 1.389;
f2 = csvread('scope_0_2_0.csv',3,0);
f2(:,1) = f2(:,1) + 1.389;

s1 = stepinfo(f1(1264:end,2),f1(1264:end,1))
s2 = stepinfo(f2(1148:end,2),f2(1148:end,1))

fig1 = figure(1);
plot(f1(1264:end,1),f1(1264:end,2),'LineWidth',2); hold on;
plot(f2(1148:end,1)-f2(1148,1)+f2(1264,1),f2(1148:end,2),'LineWidth',2);
title('Step response with CB101, 4V input');
legend('1 Stack','2 Stacks simultaneously');
xlabel('Time (s)'); ylabel('Voltage ( x1.9 kV )');
txt = ['%OS 1 stack: 17.509% '];
text(1.6,2, txt);
txt = ['%OS 2 stack: 13.161% '];
text(1.6,1.5, txt);
saveas(fig1,'CB101 Step Response.jpeg');

f3 = csvread('scope_2_1_0.csv',3,0);
f3(:,1) = f3(:,1)+1.165;
f4 = csvread('scope_2_2_0.csv',3,0);
f4(:,1) = f4(:,1)+2.6635;

s3 = stepinfo(f3(1:end,2),f3(1:end,1))
s4 = stepinfo(f4(1:end,2),f4(1:end,1))

fig2 = figure(2);
plot(f3(1201:1201+500,1),f3(1201:1201+500,2),'LineWidth',2); hold on;
plot(f4(154:154+500,1)-f4(154,1)+f3(1201,1),f4(154:154+500,2),'LineWidth',2);
title('Step response with PICO, 4V input');
legend('1 Stack','2 Stacks simultaneously','Location','northeast');
xlabel('Time (s)'); ylabel('Voltage ( x1.9 kV )');
xlim([1.2 2.2]);
ylim([-1 6])
txt = ['%OS 1 stack: 0% '];
text(1.8,2, txt);
txt = ['%OS 2 stack: 0% '];
text(1.8,1.75, txt);
saveas(fig2,'PICO Step Response.jpeg');
