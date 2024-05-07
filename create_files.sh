#!/bin/bash

# Check if the script is run as root, if not, ask for elevated permissions
if [ "$(id -u)" != "0" ]; then
   echo "This script must be run as root. Please use sudo or log in as root." 
   exit 1
fi

# Function to install required packages
install_packages() {
    echo "Checking and installing required packages..."

    # Install ImageMagick for jpg creation
    apt-get install -y imagemagick

    # Install pdflatex (part of TeX Live) for PDF creation
    apt-get install -y texlive-latex-base

    # Install ffmpeg for audio and video file creation
    apt-get install -y ffmpeg
}

# Prompt user to install required packages before proceeding
read -p "Do you want to install the necessary packages? (y/n) " answer
case ${answer:0:1} in
    y|Y )
        install_packages
    ;;
    * )
        echo "Proceeding without installing packages. The script may not work if the required packages are not installed."
    ;;
esac

# Usage: create_files.sh <type> <size_in_kb> <output_filename>

type=$1       # Type of file to create: jpg, pdf, tar, wav, mp4
size_kb=$2    # Desired size of the file in KB
output_file=$3 # Name of the output file

case $type in
  jpg)
    # Create a JPEG file using ImageMagick
    convert -size ${size_kb}x${size_kb} xc:white ${output_file}
    ;;
  pdf)
    # Create a PDF file using pdflatex
    cat <<EOF > temp.tex
\\documentclass{article}
\\usepackage{lipsum}
\\begin{document}
\\lipsum[1-50]
\\end{document}
EOF
    pdflatex temp.tex
    mv temp.pdf ${output_file}
    rm temp.tex temp.aux temp.log
    truncate -s ${size_kb}K ${output_file}
    ;;
  tar)
    # Create a TAR file using dd and tar
    dd if=/dev/zero of=dummy_file bs=1K count=${size_kb}
    tar -cf ${output_file} dummy_file
    rm dummy_file
    ;;
  wav)
    # Create a WAV audio file of specified size using ffmpeg
    duration=$((size_kb / 11))  # Approximate duration in seconds for the given size
    ffmpeg -f lavfi -i aevalsrc="sin(400*2*PI*t):s=44100" -t ${duration} -acodec pcm_s16le ${output_file}
    ;;
  mp4)
    # Create an MP4 video file of specified size using ffmpeg
    duration=$((size_kb / 50))  # Approximate duration in seconds for the given size (very rough estimation)
    ffmpeg -f lavfi -i testsrc=duration=${duration}:size=1280x720:rate=30 -c:v libx264 -pix_fmt yuv420p -t ${duration} ${output_file}
    ;;
  *)
    echo "Unsupported file type. Please choose from jpg, pdf, tar, wav, or mp4."
    exit 1
    ;;
esac

echo "${type} file created: ${output_file}, approximately ${size_kb}KB"
