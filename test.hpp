void PrintInputEvent(const Input::Event& inputEvent)
{
	switch(inputEvent.device)
	{
	case Input::Event::deviceKeyboard:
		switch(inputEvent.keyboard.type)
		{
		case Input::Event::Keyboard::typeKeyDown:
			std::cout << "KEYDOWN ";
			break;
		case Input::Event::Keyboard::typeKeyUp:
			std::cout << "KEYUP ";
			break;
		case Input::Event::Keyboard::typeKeyPress:
			std::cout << "KEYPRESS ";
			break;
		}
		std::cout << inputEvent.keyboard.key << ' ';
		break;
	case Input::Event::deviceMouse:
		// debug output
		switch(inputEvent.mouse.type)
		{
			case Input::Event::Mouse::typeButtonDown:
				std::cout << "MOUSEDOWN";
				break;
			case Input::Event::Mouse::typeButtonUp:
				std::cout << "MOUSEUP";
				break;
			case Input::Event::Mouse::typeMove:
				std::cout << "MOUSEMOVE";
				break;
		}
		std::cout << ' ';
		if(inputEvent.mouse.type == Input::Event::Mouse::typeButtonDown || inputEvent.mouse.type == Input::Event::Mouse::typeButtonUp)
		{
			switch(inputEvent.mouse.button)
			{
			case Input::Event::Mouse::buttonLeft:
				std::cout << "LEFT";
				break;
			case Input::Event::Mouse::buttonRight:
				std::cout << "RIGHT";
				break;
			case Input::Event::Mouse::buttonMiddle:
				std::cout << "MIDDLE";
				break;
			}
		}
		else
		{
			std::cout << inputEvent.mouse.offsetX << ' ' << inputEvent.mouse.offsetY << ' ' << inputEvent.mouse.offsetZ;
		}
		std::cout << ' ';
		break;
	}
}
