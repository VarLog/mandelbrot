#!/usr/bin/perl

# Read logs and plot graphics using GNUplot

use strict;
use warnings;

use Chart::Gnuplot;

# regex for input lines
my $re = qr/^\s*([\d.]+)\s+(\S+)\s+([\d.]+)\s+([\d.]+)\s*$/;

# hash for storage data
my %h_data;

while( <> ) {
  next unless /$re/;
  my ( $x, $tag, $y1, $y2 ) = ($1, $2, $3, $4);

  next unless $tag =~ /^times(\d+)$/;
  my $index = $1;

  #print "$index: $x $y1 $y2\n";
  
  push @{ $h_data{$index} }, [ $x, $y1, $y2 ];
}

# multiplot
my @charts = ();
my $m_chart = Chart::Gnuplot->new(
            output => "plot.eps",
            title  => "Multiplot",
            imagesize => "2.7, 3"
          );

# find max y for upper yrange on plot
my $max_y = 0;
for my $key( sort keys %h_data ) {
  my $a_ref = $h_data{$key};
  for my $a ( @{$a_ref} ) {
    my (undef, $y1, $y2) = @$a;
    my $y = ($y1>$y2) ? $y1 : $y2;
    $max_y = $y if $y > $max_y;
  }
}

for my $key( sort keys %h_data ) {
  my $a_ref = $h_data{$key};

  my ( @x, @y1, @y2 );

  for my $a ( @{$a_ref} ) {
    push @x,  shift @$a;
    push @y1, shift @$a;
    push @y2, shift @$a;
  }

  mkdir "./plots" unless -d "./plots";

  # Initiate the chart object
  my $chart = Chart::Gnuplot->new(
                #output => "./plots/plot${key}.png",
                #title  => "Plot #$key",
                #xlabel => "Time of the app execution",
                #ylabel => "Time of the operation",
                yrange => [0, $max_y],
                xrange => [0, '*'],
                #bg     => "white",
              );

  my $data1 = Chart::Gnuplot::DataSet->new(
                xdata => \@x,
                ydata => \@y1,
                title => 'Total time',
                style => "lines",
                linetype => 1,
                smooth => 'csplines',
                color => "red",
                width => 3,
              );
  my $data2 = Chart::Gnuplot::DataSet->new(
                xdata => \@x,
                ydata => \@y2,
                title => 'Net time',
                style => "lines",
                linetype => 1,
                smooth => 'csplines',
                color => "blue",
                width => 3,
              );
  
  ## Plot the graph
  #$chart->plot2d($data1, $data2);
  
  # Add chart to multiplot matrix
  $chart->add2d($data1, $data2);
  push @charts, [ $chart ];
}

# plot
$m_chart->multiplot(\@charts);


