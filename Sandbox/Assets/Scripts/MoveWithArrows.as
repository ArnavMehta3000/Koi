class MoveWithArrows : ScriptBehaviour
{
	float speed = 5.0f;
	void OnUpdate(float dt)
	{
		Vector3 pos = self.GetPosition();

		if (Input::IsKeyDown(KeyCode::Right)) pos.z -= speed * dt;
		if (Input::IsKeyDown(KeyCode::Left))  pos.z += speed * dt;
		if (Input::IsKeyDown(KeyCode::Up))    pos.x -= speed * dt;
		if (Input::IsKeyDown(KeyCode::Down))  pos.x += speed * dt;

		self.SetPosition(pos);
	}
}
